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
#define LOG_TAG "ExynosCameraParametersSec"
#include <log/log.h>

#include "ExynosCameraParameters.h"
#include "ExynosCameraMetadataConverter.h"

namespace android {

void ExynosCameraParameters::vendorSpecificConstructor(int cameraId)
{
    mDebugInfo.num_of_appmarker = 1; /* Default : APP4 */
    mDebugInfo.idx[0][0] = APP_MARKER_4; /* matching the app marker 4 */

    /* DebugInfo2 */
    mDebugInfo2.num_of_appmarker = 1; /* Default : APP4 */
    mDebugInfo2.idx[0][0] = APP_MARKER_4; /* matching the app marker 4 */

    mDebugInfo.debugSize[APP_MARKER_4] = sizeof(struct camera2_udm);

    mDebugInfo.debugSize[APP_MARKER_4] += sizeof(struct sensor_id_exif_data);

    mDebugInfo.debugData[APP_MARKER_4] = new char[mDebugInfo.debugSize[APP_MARKER_4]];
    memset((void *)mDebugInfo.debugData[APP_MARKER_4], 0, mDebugInfo.debugSize[APP_MARKER_4]);

    /* DebugInfo2 */
    mDebugInfo2.debugSize[APP_MARKER_4] = mDebugInfo.debugSize[APP_MARKER_4];
    mDebugInfo2.debugData[APP_MARKER_4] = new char[mDebugInfo2.debugSize[APP_MARKER_4]];
    memset((void *)mDebugInfo2.debugData[APP_MARKER_4], 0, mDebugInfo2.debugSize[APP_MARKER_4]);

    // Check debug_attribute_t struct in ExynosExif.h
    mDebugInfo.debugSize[APP_MARKER_5] = 0;

    if (mDebugInfo.idx[1][0] == APP_MARKER_5 && mDebugInfo.debugSize[APP_MARKER_5] != 0) {
        mDebugInfo.num_of_appmarker++;
        mDebugInfo.debugData[APP_MARKER_5] = new char[mDebugInfo.debugSize[APP_MARKER_5]];
        memset((void *)mDebugInfo.debugData[APP_MARKER_5], 0, mDebugInfo.debugSize[APP_MARKER_5]);

        /* DebugInfo2 */
        mDebugInfo2.idx[1][0] = APP_MARKER_5;
        mDebugInfo2.num_of_appmarker++;
        mDebugInfo2.debugSize[APP_MARKER_5] = mDebugInfo.debugSize[APP_MARKER_5];
        mDebugInfo2.debugData[APP_MARKER_5] = new char[mDebugInfo2.debugSize[APP_MARKER_5]];
        memset((void *)mDebugInfo2.debugData[APP_MARKER_5], 0, mDebugInfo2.debugSize[APP_MARKER_5]);
    }

    // CAUTION!! : Initial values must be prior to setDefaultParameter() function.
    // Initial Values : START

    m_configurations->setUseFastenAeStable(true);

    m_activeZoomRatio = 0.0f;
    m_activeZoomMargin = 0;
    m_activeZoomRect = {0, };

    m_binningProperty = 0;

    m_depthMapW = 0;
    m_depthMapH = 0;

    m_vendorConstructorInitalize(cameraId);

}

void ExynosCameraParameters::m_vendorSpecificDestructor(void)
{
    for (int i = 0; i < mDebugInfo.num_of_appmarker; i++) {
        if (mDebugInfo.debugData[mDebugInfo.idx[i][0]])
            delete[] mDebugInfo.debugData[mDebugInfo.idx[i][0]];
        mDebugInfo.debugData[mDebugInfo.idx[i][0]] = NULL;
        mDebugInfo.debugSize[mDebugInfo.idx[i][0]] = 0;
    }

    for (int i = 0; i < mDebugInfo2.num_of_appmarker; i++) {
        if (mDebugInfo2.debugData[mDebugInfo2.idx[i][0]])
            delete[] mDebugInfo2.debugData[mDebugInfo2.idx[i][0]];
        mDebugInfo2.debugData[mDebugInfo2.idx[i][0]] = NULL;
        mDebugInfo2.debugSize[mDebugInfo2.idx[i][0]] = 0;
    }
}

status_t ExynosCameraParameters::setSize(enum HW_INFO_SIZE_TYPE type, uint32_t width, uint32_t height, int outputPortId)
{
    status_t ret = NO_ERROR;

    switch(type) {
    case HW_INFO_HW_YUV_SIZE:
    {
        int widthArrayNum = sizeof(m_hwYuvWidth)/sizeof(m_hwYuvWidth[0]);
        int heightArrayNum = sizeof(m_hwYuvHeight)/sizeof(m_hwYuvHeight[0]);

        if (widthArrayNum != YUV_OUTPUT_PORT_ID_MAX
            || heightArrayNum != YUV_OUTPUT_PORT_ID_MAX) {
            android_printAssert(NULL, LOG_TAG, "ASSERT:Invalid yuvSize array length %dx%d."\
                    " YUV_OUTPUT_PORT_ID_MAX %d",
                    widthArrayNum, heightArrayNum,
                    YUV_OUTPUT_PORT_ID_MAX);
            return INVALID_OPERATION;
        }

        if (outputPortId < 0) {
            CLOGE("Invalid outputPortId(%d)", outputPortId);
            return INVALID_OPERATION;
        }

        m_hwYuvWidth[outputPortId] = width;
        m_hwYuvHeight[outputPortId] = height;
        break;
    }
    case HW_INFO_HW_BNS_SIZE:
        m_width[type] = width;
        m_height[type] = height;
        updateHwSensorSize();
        break;
    case HW_INFO_MAX_HW_YUV_SIZE:
    case HW_INFO_HW_SENSOR_SIZE:
    case HW_INFO_HW_PICTURE_SIZE:
        m_width[type] = width;
        m_height[type] = height;
        break;
    default:
        break;
    }

    return ret;
}

status_t ExynosCameraParameters::getSize(enum HW_INFO_SIZE_TYPE type, uint32_t *width, uint32_t *height, int outputPortId)
{
    status_t ret = NO_ERROR;

    switch(type) {
    case HW_INFO_SENSOR_MARGIN_SIZE:
        *width = m_staticInfo->sensorMarginW;
        *height = m_staticInfo->sensorMarginH;
        break;
    case HW_INFO_MAX_SENSOR_SIZE:
        *width = m_staticInfo->maxSensorW;
        *height = m_staticInfo->maxSensorH;
        break;
    case HW_INFO_MAX_PREVIEW_SIZE:
        *width = m_staticInfo->maxPreviewW;
        *height = m_staticInfo->maxPreviewH;
        break;
    case HW_INFO_MAX_PICTURE_SIZE:
        *width = m_staticInfo->maxPictureW;
        *height = m_staticInfo->maxPictureH;
        break;
    case HW_INFO_MAX_THUMBNAIL_SIZE:
        *width = m_staticInfo->maxThumbnailW;
        *height = m_staticInfo->maxThumbnailH;
        break;
    case HW_INFO_HW_YUV_SIZE:
    {
        int widthArrayNum = sizeof(m_hwYuvWidth)/sizeof(m_hwYuvWidth[0]);
        int heightArrayNum = sizeof(m_hwYuvHeight)/sizeof(m_hwYuvHeight[0]);

        if (widthArrayNum != YUV_OUTPUT_PORT_ID_MAX
            || heightArrayNum != YUV_OUTPUT_PORT_ID_MAX) {
            android_printAssert(NULL, LOG_TAG, "ASSERT:Invalid yuvSize array length %dx%d."\
                    " YUV_OUTPUT_PORT_ID_MAX %d",
                    widthArrayNum, heightArrayNum,
                    YUV_OUTPUT_PORT_ID_MAX);
            return INVALID_OPERATION;
        }

        if (outputPortId < 0) {
            CLOGE("Invalid outputPortId(%d)", outputPortId);
            return INVALID_OPERATION;
        }

        *width = m_hwYuvWidth[outputPortId];
        *height = m_hwYuvHeight[outputPortId];
        break;
    }
    case HW_INFO_HW_SENSOR_SIZE:
    {
        int sizeList[SIZE_LUT_INDEX_END];
        /* matched ratio LUT is not existed, use equation */
        if (m_useSizeTable == true
            && m_getPreviewSizeList(sizeList) == NO_ERROR) {

            *width = sizeList[SENSOR_W];
            *height = sizeList[SENSOR_H];
            break;
        }
    }
    case HW_INFO_MAX_HW_YUV_SIZE:
    case HW_INFO_HW_BNS_SIZE:
    case HW_INFO_HW_PICTURE_SIZE:
        *width = m_width[type];
        *height = m_height[type];
        break;
    case HW_INFO_HW_PREVIEW_SIZE:
    {
        int previewPort = -1;
        enum HW_INFO_SIZE_TYPE sizeType = HW_INFO_MAX_PREVIEW_SIZE;

        previewPort = getPreviewPortId();

        if (previewPort >= YUV_0 && previewPort < YUV_MAX) {
            sizeType = HW_INFO_HW_YUV_SIZE;
        } else {
            CLOGE("Invalid port Id. Set to default yuv size.");
            previewPort = -1;
        }

        getSize(sizeType, width, height, previewPort);
        break;
    }
    default:
        break;
    }

    return ret;
}

status_t ExynosCameraParameters::resetSize(enum HW_INFO_SIZE_TYPE type)
{
    status_t ret = NO_ERROR;

    switch(type) {
    case HW_INFO_HW_YUV_SIZE:
        memset(m_hwYuvWidth, 0, sizeof(m_hwYuvWidth));
        memset(m_hwYuvHeight, 0, sizeof(m_hwYuvHeight));
        break;
    case HW_INFO_MAX_HW_YUV_SIZE:
        m_width[type] = 0;
        m_height[type] = 0;
        break;
    default:
        break;
    }

    return ret;
}

bool ExynosCameraParameters::isSupportedFunction(enum SUPPORTED_HW_FUNCTION_TYPE type) const
{
    bool functionSupport = false;

    switch(type) {
    case SUPPORTED_HW_FUNCTION_SENSOR_STANDBY:
        if (m_cameraId == CAMERA_ID_BACK || m_cameraId == CAMERA_ID_FRONT) {
#ifdef SUPPORT_MASTER_SENSOR_STANDBY
            functionSupport = SUPPORT_MASTER_SENSOR_STANDBY;
#endif
        } else if (m_cameraId == CAMERA_ID_BACK_1 || m_cameraId == CAMERA_ID_FRONT_1) {
#ifdef SUPPORT_SLAVE_SENSOR_STANDBY
            functionSupport = SUPPORT_SLAVE_SENSOR_STANDBY;
#endif
        }
        break;
    default:
        break;
    }

    return functionSupport;
}

status_t ExynosCameraParameters::m_getPreviewSizeList(int *sizeList)
{
    int *tempSizeList = NULL;
    int (*previewSizelist)[SIZE_OF_LUT] = NULL;
    int previewSizeLutMax = 0;
    int configMode = -1;
    int videoRatioEnum = SIZE_RATIO_16_9;
    int index = 0;

#ifdef USE_BINNING_MODE
    if (getBinningMode() == true) {
        tempSizeList = getBinningSizeTable();
        if (tempSizeList == NULL) {
            CLOGE(" getBinningSizeTable is NULL");
            return INVALID_OPERATION;
        }
    } else
#endif
    {
        configMode = m_configurations->getConfigMode();
        switch (configMode) {
        case CONFIG_MODE::NORMAL:
            {
                if (m_configurations->getMode(CONFIGURATION_PIP_MODE) == true) {
                    previewSizelist = m_staticInfo->dualPreviewSizeLut;
                    previewSizeLutMax = m_staticInfo->dualPreviewSizeLutMax;
                } else {
                    if (m_configurations->getSamsungCamera()) {
                        previewSizelist = m_staticInfo->previewSizeLut;
                        previewSizeLutMax = m_staticInfo->previewSizeLutMax;
                    } else {
                        previewSizelist = m_staticInfo->previewFullSizeLut;
                        previewSizeLutMax = m_staticInfo->previewFullSizeLutMax;
                    }
                }

                if (previewSizelist == NULL) {
                    CLOGE("previewSizeLut is NULL");
                    return INVALID_OPERATION;
                }

                if (m_getSizeListIndex(previewSizelist, previewSizeLutMax, m_cameraInfo.yuvSizeRatioId, &m_cameraInfo.yuvSizeLutIndex) != NO_ERROR) {
                    CLOGE("unsupported preview ratioId(%d)", m_cameraInfo.yuvSizeRatioId);
                    return BAD_VALUE;
                }

                tempSizeList = previewSizelist[m_cameraInfo.yuvSizeLutIndex];
            }
            break;
        case CONFIG_MODE::HIGHSPEED_60:
            {
                if (m_staticInfo->videoSizeLutHighSpeed60Max == 0
                        || m_staticInfo->videoSizeLutHighSpeed60 == NULL) {
                    CLOGE("videoSizeLutHighSpeed60 is NULL");
                } else {
                    for (index = 0; index < m_staticInfo->videoSizeLutHighSpeed60Max; index++) {
                        if (m_staticInfo->videoSizeLutHighSpeed60[index][RATIO_ID] == videoRatioEnum) {
                            break;
                        }
                    }

                    if (index >= m_staticInfo->videoSizeLutHighSpeed60Max)
                        index = 0;

                    tempSizeList = m_staticInfo->videoSizeLutHighSpeed60[index];
                }
            }
            break;
        case CONFIG_MODE::HIGHSPEED_120:
            {
                if (m_staticInfo->videoSizeLutHighSpeed120Max == 0
                        || m_staticInfo->videoSizeLutHighSpeed120 == NULL) {
                     CLOGE(" videoSizeLutHighSpeed120 is NULL");
                } else {
                    for (index = 0; index < m_staticInfo->videoSizeLutHighSpeed120Max; index++) {
                        if (m_staticInfo->videoSizeLutHighSpeed120[index][RATIO_ID] == videoRatioEnum) {
                            break;
                        }
                    }

                    if (index >= m_staticInfo->videoSizeLutHighSpeed120Max)
                        index = 0;

                    tempSizeList = m_staticInfo->videoSizeLutHighSpeed120[index];
                }
            }
            break;
        case CONFIG_MODE::HIGHSPEED_240:
            {
                if (m_staticInfo->videoSizeLutHighSpeed240Max == 0
                        || m_staticInfo->videoSizeLutHighSpeed240 == NULL) {
                     CLOGE(" videoSizeLutHighSpeed240 is NULL");
                } else {
                    for (index = 0; index < m_staticInfo->videoSizeLutHighSpeed240Max; index++) {
                        if (m_staticInfo->videoSizeLutHighSpeed240[index][RATIO_ID] == videoRatioEnum) {
                            break;
                        }
                    }

                    if (index >= m_staticInfo->videoSizeLutHighSpeed240Max)
                        index = 0;

                    tempSizeList = m_staticInfo->videoSizeLutHighSpeed240[index];
                }
            }
            break;
        case CONFIG_MODE::HIGHSPEED_480:
            {
                if (m_staticInfo->videoSizeLutHighSpeed480Max == 0
                        || m_staticInfo->videoSizeLutHighSpeed480 == NULL) {
                    CLOGE(" videoSizeLutHighSpeed480 is NULL");
                } else {
                    for (index = 0; index < m_staticInfo->videoSizeLutHighSpeed480Max; index++) {
                        if (m_staticInfo->videoSizeLutHighSpeed480[index][RATIO_ID] == videoRatioEnum) {
                            break;
                        }
                    }

                    if (index >= m_staticInfo->videoSizeLutHighSpeed480Max)
                        index = 0;

                    tempSizeList = m_staticInfo->videoSizeLutHighSpeed480[index];
                }
            }
            break;
#ifdef SAMSUNG_SSM
        case CONFIG_MODE::SSM_240:
            {
                if (m_staticInfo->videoSizeLutSSMMax == 0
                        || m_staticInfo->videoSizeLutSSM == NULL) {
                     CLOGE(" videoSizeLutSSMMax is NULL");
                } else {
                    for (index = 0; index < m_staticInfo->videoSizeLutSSMMax; index++) {
                        if (m_staticInfo->videoSizeLutSSM[index][RATIO_ID] == videoRatioEnum) {
                            break;
                        }
                    }

                    if (index >= m_staticInfo->videoSizeLutSSMMax)
                        index = 0;

                    tempSizeList = m_staticInfo->videoSizeLutSSM[index];
                }
            }
            break;
#endif
        }
    }

    if (tempSizeList == NULL) {
         CLOGE(" fail to get LUT");
        return INVALID_OPERATION;
    }

    for (int i = 0; i < SIZE_LUT_INDEX_END; i++)
        sizeList[i] = tempSizeList[i];

    return NO_ERROR;
}

status_t ExynosCameraParameters::m_getPictureSizeList(int *sizeList)
{
    int *tempSizeList = NULL;
    int (*pictureSizelist)[SIZE_OF_LUT] = NULL;
    int pictureSizelistMax = 0;

    if (m_configurations->getSamsungCamera()
        || m_configurations->getMode(CONFIGURATION_PIP_MODE) == true) {
        pictureSizelist = m_staticInfo->pictureSizeLut;
        pictureSizelistMax = m_staticInfo->pictureSizeLutMax;
    } else {
        pictureSizelist = m_staticInfo->pictureFullSizeLut;
        pictureSizelistMax = m_staticInfo->pictureFullSizeLutMax;
    }

    if (pictureSizelist == NULL) {
        CLOGE("pictureSizelist is NULL");
        return INVALID_OPERATION;
    }

    if (m_getSizeListIndex(pictureSizelist, pictureSizelistMax, m_cameraInfo.pictureSizeRatioId, &m_cameraInfo.pictureSizeLutIndex) != NO_ERROR) {
        CLOGE("unsupported picture ratioId(%d)", m_cameraInfo.pictureSizeRatioId);
        return BAD_VALUE;
    }

    tempSizeList = pictureSizelist[m_cameraInfo.pictureSizeLutIndex];

    if (tempSizeList == NULL) {
         CLOGE(" fail to get LUT");
        return INVALID_OPERATION;
    }

    for (int i = 0; i < SIZE_LUT_INDEX_END; i++)
        sizeList[i] = tempSizeList[i];

    return NO_ERROR;
}

bool ExynosCameraParameters::m_isSupportedFullSizePicture(void)
{
    /* To support multi ratio picture, use size of picture as full size. */
    return true;
}

status_t ExynosCameraParameters::getPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int hwBnsW   = 0;
    int hwBnsH   = 0;
    int hwBcropW = 0;
    int hwBcropH = 0;
    int hwSensorMarginW = 0;
    int hwSensorMarginH = 0;
    int sizeList[SIZE_LUT_INDEX_END];

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == false
        || m_getPictureSizeList(sizeList) != NO_ERROR
        || m_isSupportedFullSizePicture() == false)
        return calcPictureBayerCropSize(srcRect, dstRect);

    /* use LUT */
    hwBnsW   = sizeList[BNS_W];
    hwBnsH   = sizeList[BNS_H];
    hwBcropW = sizeList[BCROP_W];
    hwBcropH = sizeList[BCROP_H];

    /* Re-calculate the BNS size for removing Sensor Margin.
       On Capture Stream(3AA_M2M_Input), the BNS is not used.
       So, the BNS ratio is not needed to be considered for sensor margin
     */
    getSize(HW_INFO_SENSOR_MARGIN_SIZE, (uint32_t *)&hwSensorMarginW, (uint32_t *)&hwSensorMarginH);
    hwBnsW = hwBnsW - hwSensorMarginW;
    hwBnsH = hwBnsH - hwSensorMarginH;

    /* src */
    srcRect->x = 0;
    srcRect->y = 0;
    srcRect->w = hwBnsW;
    srcRect->h = hwBnsH;

    ExynosRect activeArraySize;
    ExynosRect cropRegion;
    ExynosRect hwActiveArraySize;
    float scaleRatioW = 0.0f, scaleRatioH = 0.0f;
    status_t ret = NO_ERROR;

    getSize(HW_INFO_MAX_SENSOR_SIZE, (uint32_t *)&activeArraySize.w, (uint32_t *)&activeArraySize.h);

    if (isUseReprocessing3aaInputCrop() == true) {
#ifdef SAMSUNG_HIFI_CAPTURE
        if (m_configurations->getDynamicMode(DYNAMIC_HIFI_CAPTURE_MODE)
            && getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON) {
            cropRegion.x = 0;
            cropRegion.y = 0;
            getSize(HW_INFO_MAX_SENSOR_SIZE, (uint32_t *)&cropRegion.w, (uint32_t *)&cropRegion.h);
        } else
#endif
        {
            m_getCropRegion(&cropRegion.x, &cropRegion.y, &cropRegion.w, &cropRegion.h);
        }
    }

    CLOGV("ActiveArraySize %dx%d(%d) CropRegion %d,%d %dx%d(%d) HWSensorSize %dx%d(%d) BcropSize %dx%d(%d)",
            activeArraySize.w, activeArraySize.h, SIZE_RATIO(activeArraySize.w, activeArraySize.h),
            cropRegion.x, cropRegion.y, cropRegion.w, cropRegion.h, SIZE_RATIO(cropRegion.w, cropRegion.h),
            hwBnsW, hwBnsH, SIZE_RATIO(hwBnsW, hwBnsH),
            hwBcropW, hwBcropH, SIZE_RATIO(hwBcropW, hwBcropH));

    /* Calculate H/W active array size for current sensor aspect ratio
       based on active array size
     */
    ret = getCropRectAlign(activeArraySize.w, activeArraySize.h,
                           hwBnsW, hwBnsH,
                           &hwActiveArraySize.x, &hwActiveArraySize.y,
                           &hwActiveArraySize.w, &hwActiveArraySize.h,
                           2, 2, 1.0f);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getCropRectAlign. Src %dx%d, Dst %dx%d",
                activeArraySize.w, activeArraySize.h,
                hwBnsW, hwBnsH);
        return INVALID_OPERATION;
    }

    /* Scale down the crop region & HW active array size
       to adjust it to the 3AA input size without sensor margin
     */
    scaleRatioW = (float) hwBnsW / (float) hwActiveArraySize.w;
    scaleRatioH = (float) hwBnsH / (float) hwActiveArraySize.h;

    cropRegion.x = ALIGN_DOWN((int) (((float) cropRegion.x) * scaleRatioW), 2);
    cropRegion.y = ALIGN_DOWN((int) (((float) cropRegion.y) * scaleRatioH), 2);
    cropRegion.w = (int) ((float) cropRegion.w) * scaleRatioW;
    cropRegion.h = (int) ((float) cropRegion.h) * scaleRatioH;

    hwActiveArraySize.x = ALIGN_DOWN((int) (((float) hwActiveArraySize.x) * scaleRatioW), 2);
    hwActiveArraySize.y = ALIGN_DOWN((int) (((float) hwActiveArraySize.y) * scaleRatioH), 2);
    hwActiveArraySize.w = (int) (((float) hwActiveArraySize.w) * scaleRatioW);
    hwActiveArraySize.h = (int) (((float) hwActiveArraySize.h) * scaleRatioH);

    if (cropRegion.w < 1 || cropRegion.h < 1) {
        CLOGW("Invalid scaleRatio %fx%f, cropRegion' %d,%d %dx%d.",
                scaleRatioW, scaleRatioH,
                cropRegion.x, cropRegion.y, cropRegion.w, cropRegion.h);
        cropRegion.x = 0;
        cropRegion.y = 0;
        cropRegion.w = hwBnsW;
        cropRegion.h = hwBnsH;
    }

    /* Calculate HW bcrop size inside crop region' */
    if ((cropRegion.w > hwBcropW) && (cropRegion.h > hwBcropH)) {
        dstRect->x = ALIGN_DOWN(((cropRegion.w - hwBcropW) >> 1), 2);
        dstRect->y = ALIGN_DOWN(((cropRegion.h - hwBcropH) >> 1), 2);
        dstRect->w = hwBcropW;
        dstRect->h = hwBcropH;
    } else {
        ret = getCropRectAlign(cropRegion.w, cropRegion.h,
                hwBcropW, hwBcropH,
                &(dstRect->x), &(dstRect->y),
                &(dstRect->w), &(dstRect->h),
                CAMERA_BCROP_ALIGN, 2, 1.0f);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getCropRectAlign. Src %dx%d, Dst %dx%d",
                    cropRegion.w, cropRegion.h,
                    hwBcropW, hwBcropH);
            return INVALID_OPERATION;
        }

        dstRect->x = ALIGN_DOWN(dstRect->x, 2);
        dstRect->y = ALIGN_DOWN(dstRect->y, 2);
    }

    /* Add crop region offset to HW bcrop offset */
    dstRect->x += cropRegion.x;
    dstRect->y += cropRegion.y;

    /* Check the boundary of HW active array size for HW bcrop offset & size */
    if (dstRect->x < hwActiveArraySize.x) dstRect->x = hwActiveArraySize.x;
    if (dstRect->y < hwActiveArraySize.y) dstRect->y = hwActiveArraySize.y;
    if (dstRect->x + dstRect->w > hwActiveArraySize.x + hwBnsW)
        dstRect->x = hwActiveArraySize.x + hwBnsW - dstRect->w;
    if (dstRect->y + dstRect->h > hwActiveArraySize.y + hwBnsH)
        dstRect->y = hwActiveArraySize.y + hwBnsH - dstRect->h;

    /* Remove HW active array size offset */
    dstRect->x -= hwActiveArraySize.x;
    dstRect->y -= hwActiveArraySize.y;

    CLOGV("HWBcropSize %d,%d %dx%d(%d)",
            dstRect->x, dstRect->y, dstRect->w, dstRect->h, SIZE_RATIO(dstRect->w, dstRect->h));

    /* Compensate the crop size to satisfy Max Scale Up Ratio */
    if (dstRect->w * SCALER_MAX_SCALE_UP_RATIO < hwBnsW
        || dstRect->h * SCALER_MAX_SCALE_UP_RATIO < hwBnsH) {
        dstRect->w = ALIGN_UP((int)ceil((float)hwBnsW / SCALER_MAX_SCALE_UP_RATIO), CAMERA_BCROP_ALIGN);
        dstRect->h = ALIGN_UP((int)ceil((float)hwBnsH / SCALER_MAX_SCALE_UP_RATIO), CAMERA_BCROP_ALIGN);
    }

#if DEBUG_PERFRAME
    CLOGD("hwBnsSize %dx%d, hwBcropSize %d,%d %dx%d",
            srcRect->w, srcRect->h,
            dstRect->x, dstRect->y, dstRect->w, dstRect->h);
#endif

    return NO_ERROR;
}

status_t ExynosCameraParameters::m_getPreviewBdsSize(ExynosRect *dstRect)
{
    int hwBdsW = 0;
    int hwBdsH = 0;
    int sizeList[SIZE_LUT_INDEX_END];
    int maxYuvW = 0, maxYuvH = 0;

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == false
        || m_getPreviewSizeList(sizeList) != NO_ERROR) {
        ExynosRect rect;
        return calcPreviewBDSSize(&rect, dstRect);
    }

    /* use LUT */
    hwBdsW = sizeList[BDS_W];
    hwBdsH = sizeList[BDS_H];

    getSize(HW_INFO_MAX_HW_YUV_SIZE, (uint32_t *)&maxYuvW, (uint32_t *)&maxYuvH);

    if ((getCameraId() == CAMERA_ID_BACK)
#ifdef USE_DUAL_CAMERA
        || (getCameraId() == CAMERA_ID_BACK_1)
#endif
        ) {
        if (m_configurations->getSamsungCamera()) {
#ifdef USE_BDS_IN_UHD_VIDEO
            if (m_configurations->getMode(CONFIGURATION_RECORDING_MODE) == true) {
                int videoSizeW = 0, videoSizeH = 0;
                m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&videoSizeW, (uint32_t *)&videoSizeH);

                if (videoSizeW >= UHD_DVFS_CEILING_WIDTH || videoSizeH >= UHD_DVFS_CEILING_HEIGHT) {
                    hwBdsW = MAX(videoSizeW, maxYuvW);
                    hwBdsH = MAX(videoSizeH, maxYuvH);
                } else if (maxYuvW > hwBdsW || maxYuvH > hwBdsH) {
                    hwBdsW = maxYuvW;
                    hwBdsH = maxYuvH;
                }
            } else
#endif
            {
                if (maxYuvW > hwBdsW || maxYuvH > hwBdsH) {
                    hwBdsW = maxYuvW;
                    hwBdsH = maxYuvH;
                }
            }

            if (hwBdsW > sizeList[BCROP_W] || hwBdsH > sizeList[BCROP_H]) {
                hwBdsW = sizeList[BCROP_W];
                hwBdsH = sizeList[BCROP_H];
            }
        } else {
            if (maxYuvW > UHD_DVFS_CEILING_WIDTH || maxYuvH > UHD_DVFS_CEILING_HEIGHT) {
                hwBdsW = sizeList[BCROP_W];
                hwBdsH = sizeList[BCROP_H];
                m_configurations->setMode(CONFIGURATION_FULL_SIZE_LUT_MODE, true);
            } else if (maxYuvW > sizeList[BDS_W] || maxYuvH > sizeList[BDS_H]) {
                hwBdsW = UHD_DVFS_CEILING_WIDTH;
                hwBdsH = UHD_DVFS_CEILING_HEIGHT;
                m_configurations->setMode(CONFIGURATION_FULL_SIZE_LUT_MODE, true);
            }
        }
    } else {
        if (m_configurations->getSamsungCamera()) {
            if (maxYuvW > hwBdsW || maxYuvH > hwBdsH) {
                hwBdsW = maxYuvW;
                hwBdsH = maxYuvH;
                m_configurations->setMode(CONFIGURATION_FULL_SIZE_LUT_MODE, true);
            }

            if (hwBdsW > sizeList[BCROP_W] || hwBdsH > sizeList[BCROP_H]) {
                hwBdsW = sizeList[BCROP_W];
                hwBdsH = sizeList[BCROP_H];
            }
        } else {
            if (maxYuvW > sizeList[BDS_W] || maxYuvH > sizeList[BDS_H]) {
                hwBdsW = sizeList[BCROP_W];
                hwBdsH = sizeList[BCROP_H];
                m_configurations->setMode(CONFIGURATION_FULL_SIZE_LUT_MODE, true);
            }
        }
    }

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = hwBdsW;
    dstRect->h = hwBdsH;

#ifdef DEBUG_PERFRAME
    CLOGD("hwBdsSize (%dx%d)", dstRect->w, dstRect->h);
#endif

    return NO_ERROR;
}

status_t ExynosCameraParameters::checkYuvSize(const int width, const int height, const int outputPortId, bool reprocessing)
{
    int curYuvWidth = 0;
    int curYuvHeight = 0;
    uint32_t minYuvW = 0, minYuvH = 0;
    uint32_t maxYuvW = 0, maxYuvH = 0;
    int curRatio = SIZE_RATIO_16_9;
    bool ret = true;

    m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&curYuvWidth, (uint32_t *)&curYuvHeight, outputPortId);

    if (reprocessing) {
        ret = m_isSupportedPictureSize(width, height);
    } else {
        ret = m_isSupportedYuvSize(width, height, outputPortId, &curRatio);
    }

    if (!ret) {
         CLOGE("Invalid YUV size. %dx%d", width, height);
        return BAD_VALUE;
    }

    CLOGI("curYuvSize %dx%d newYuvSize %dx%d outputPortId %d curRatio(%d)",
            curYuvWidth, curYuvHeight, width, height, outputPortId, curRatio);

    if (curYuvWidth != width || curYuvHeight != height) {
        m_configurations->setSize(CONFIGURATION_YUV_SIZE, width, height, outputPortId);
    }

    if (outputPortId < YUV_MAX) {
        /* Update minimum YUV size */
        m_configurations->getSize(CONFIGURATION_MIN_YUV_SIZE, &minYuvW, &minYuvH);
        if (minYuvW == 0) {
            m_configurations->setSize(CONFIGURATION_MIN_YUV_SIZE, width, minYuvH);
        } else if ((uint32_t)width < minYuvW) {
            m_configurations->setSize(CONFIGURATION_MIN_YUV_SIZE, width, minYuvH);
        }

        m_configurations->getSize(CONFIGURATION_MIN_YUV_SIZE, &minYuvW, &minYuvH);
        if (minYuvH == 0) {
            m_configurations->setSize(CONFIGURATION_MIN_YUV_SIZE, minYuvW, height);
        } else if ((uint32_t)height < minYuvH) {
            m_configurations->setSize(CONFIGURATION_MIN_YUV_SIZE, minYuvW, height);
        }

        /* Update maximum YUV size */
        m_configurations->getSize(CONFIGURATION_MAX_YUV_SIZE, &maxYuvW, &maxYuvH);
        if(maxYuvW == 0) {
            m_configurations->setSize(CONFIGURATION_MAX_YUV_SIZE, width, maxYuvH);
            m_cameraInfo.yuvSizeRatioId = curRatio;
        } else if ((uint32_t)width >= maxYuvW) {
            m_configurations->setSize(CONFIGURATION_MAX_YUV_SIZE, width, maxYuvH);
            m_cameraInfo.yuvSizeRatioId = curRatio;
        }

        m_configurations->getSize(CONFIGURATION_MAX_YUV_SIZE, &maxYuvW, &maxYuvH);
        if(maxYuvH == 0) {
            m_configurations->setSize(CONFIGURATION_MAX_YUV_SIZE, maxYuvW, height);
            m_cameraInfo.yuvSizeRatioId = curRatio;
        } else if ((uint32_t)height >= maxYuvH) {
            m_configurations->setSize(CONFIGURATION_MAX_YUV_SIZE, maxYuvW, height);
            m_cameraInfo.yuvSizeRatioId = curRatio;
        }

        m_configurations->getSize(CONFIGURATION_MIN_YUV_SIZE, &minYuvW, &minYuvH);
        m_configurations->getSize(CONFIGURATION_MAX_YUV_SIZE, &maxYuvW, &maxYuvH);
        CLOGV("m_minYuvW(%d) m_minYuvH(%d) m_maxYuvW(%d) m_maxYuvH(%d)",
            minYuvW, minYuvH, maxYuvW, maxYuvH);
    }

    return NO_ERROR;
}

status_t ExynosCameraParameters::checkHwYuvSize(const int hwWidth, const int hwHeight, const int outputPortId, __unused bool reprocessing)
{
    int curHwYuvWidth = 0;
    int curHwYuvHeight = 0;
    uint32_t maxHwYuvW = 0, maxHwYuvH = 0;

    getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&curHwYuvWidth, (uint32_t *)&curHwYuvHeight, outputPortId);

    CLOGI("curHwYuvSize %dx%d newHwYuvSize %dx%d outputPortId %d",
            curHwYuvWidth, curHwYuvHeight, hwWidth, hwHeight, outputPortId);

    if (curHwYuvWidth != hwWidth || curHwYuvHeight != hwHeight) {
        setSize(HW_INFO_HW_YUV_SIZE, hwWidth, hwHeight, outputPortId);
    }

    if (outputPortId < YUV_MAX) {
        /* Update maximum YUV size */
        getSize(HW_INFO_MAX_HW_YUV_SIZE, &maxHwYuvW, &maxHwYuvH);
        if ((uint32_t)hwWidth > maxHwYuvW) {
            setSize(HW_INFO_MAX_HW_YUV_SIZE, hwWidth, maxHwYuvH);
        }

        getSize(HW_INFO_MAX_HW_YUV_SIZE, &maxHwYuvW, &maxHwYuvH);
        if ((uint32_t)hwHeight > maxHwYuvH) {
            setSize(HW_INFO_MAX_HW_YUV_SIZE, maxHwYuvW, hwHeight);
        }

        getSize(HW_INFO_MAX_HW_YUV_SIZE, &maxHwYuvW, &maxHwYuvH);
        CLOGV("m_maxYuvW(%d) m_maxYuvH(%d)", maxHwYuvW, maxHwYuvH);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::getYuvVendorSize(int *width, int *height, int index, __unused ExynosRect ispSize)
{
    int widthArrayNum = sizeof(m_cameraInfo.hwYuvWidth)/sizeof(m_cameraInfo.hwYuvWidth[0]);
    int heightArrayNum = sizeof(m_cameraInfo.hwYuvHeight)/sizeof(m_cameraInfo.hwYuvHeight[0]);

    if (widthArrayNum != YUV_OUTPUT_PORT_ID_MAX
            || heightArrayNum != YUV_OUTPUT_PORT_ID_MAX) {
        android_printAssert(NULL, LOG_TAG, "ASSERT:Invalid yuvSize array length %dx%d."\
                " YUV_OUTPUT_PORT_ID_MAX %d",
                widthArrayNum, heightArrayNum,
                YUV_OUTPUT_PORT_ID_MAX);
        return;
    }

    if ((index >= ExynosCameraParameters::YUV_STALL_0)
        && (m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT) == (index % ExynosCameraParameters::YUV_MAX))) {
        if (m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE) == YUV_STALL_USAGE_DSCALED) {
            m_configurations->getSize(CONFIGURATION_DS_YUV_STALL_SIZE, (uint32_t *)width, (uint32_t *)height);
        } else {
#ifdef SAMSUNG_HIFI_CAPTURE
            if (m_configurations->getDynamicMode(DYNAMIC_HIFI_CAPTURE_MODE) == true) {
                if (getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON) {
                    *width = ALIGN_UP(ispSize.w, 16);
                    *height = ALIGN_UP(ispSize.h, 16);
                } else {
                    getSize(HW_INFO_HW_PICTURE_SIZE, (uint32_t *)width, (uint32_t *)height);
                }
            } else
#endif
#ifdef SAMSUNG_STR_CAPTURE
            if (m_configurations->getMode(CONFIGURATION_STR_CAPTURE_MODE)) {
                m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)width, (uint32_t *)height);
            } else
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY_SNAPSHOT
            if (m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_SNAPSHOT_MODE)) {
                m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)width, (uint32_t *)height);
            } else
#endif
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
            if (m_configurations->getMode(CONFIGURATION_FUSION_CAPTURE_MODE)) {
                m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)width, (uint32_t *)height);
            } else
#endif
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
            if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_PORTRAIT
                || m_configurations->getScenario() == SCENARIO_DUAL_FRONT_PORTRAIT) {
                m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)width, (uint32_t *)height);
            } else
#endif
            {
                getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)width, (uint32_t *)height, index);
            }
        }
    } else {
        getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)width, (uint32_t *)height, index);
    }
}

#ifdef SUPPORT_DEPTH_MAP
bool ExynosCameraParameters::isDepthMapSupported(void) {
    if (m_configurations->getSamsungCamera() == true
        && m_staticInfo->depthMapSizeLut!= NULL) {
        return true;
    }

    return false;
}

status_t ExynosCameraParameters::getDepthMapSize(int *depthMapW, int *depthMapH)
{
    /* set by HAL if Depth Stream does not exist */
    if (isDepthMapSupported() == true && m_depthMapW == 0 && m_depthMapH == 0) {
        for (int index = 0; index < m_staticInfo->depthMapSizeLutMax; index++) {
            if (m_staticInfo->depthMapSizeLut[index][RATIO_ID] == m_cameraInfo.yuvSizeRatioId) {
                *depthMapW = m_staticInfo->depthMapSizeLut[index][SENSOR_W];
                *depthMapH = m_staticInfo->depthMapSizeLut[index][SENSOR_H];
                return NO_ERROR;
            }
        }
    }

    /* set by user */
    *depthMapW = m_depthMapW;
    *depthMapH = m_depthMapH;

    return NO_ERROR;
}

void ExynosCameraParameters::setDepthMapSize(int depthMapW, int depthMapH)
{
    m_depthMapW = depthMapW;
    m_depthMapH = depthMapH;
}
#endif

void ExynosCameraParameters::m_setExifChangedAttribute(exif_attribute_t *exifInfo,
                                                        ExynosRect       *pictureRect,
                                                        ExynosRect       *thumbnailRect,
                                                        camera2_shot_t   *shot,
                                                        bool                useDebugInfo2)
{
    unsigned int offset = 0;
    bool isWriteExif = false;
    uint64_t exposureTimeCapture = 0L;
    debug_attribute_t &debugInfo = ((useDebugInfo2 == false) ? mDebugInfo : mDebugInfo2);

    m_staticInfoExifLock.lock();
    /* Maker Note */
    /* Clear previous debugInfo data */
    memset((void *)debugInfo.debugData[APP_MARKER_4], 0, debugInfo.debugSize[APP_MARKER_4]);
    /* back-up udm info for exif's maker note */
    memcpy((void *)debugInfo.debugData[APP_MARKER_4], (void *)&shot->udm, sizeof(struct camera2_udm));
    offset = sizeof(struct camera2_udm);

	m_vendorSetExifChangedAttribute(debugInfo, offset, isWriteExif, shot, useDebugInfo2);

    m_staticInfoExifLock.unlock();

    if (exifInfo == NULL || pictureRect == NULL || thumbnailRect == NULL) {
        return;
    }

    /* JPEG Picture Size */
    exifInfo->width = pictureRect->w;
    exifInfo->height = pictureRect->h;

    /* Orientation */
    switch (shot->ctl.jpeg.orientation) {
        case 90:
            exifInfo->orientation = EXIF_ORIENTATION_90;
            break;
        case 180:
            exifInfo->orientation = EXIF_ORIENTATION_180;
            break;
        case 270:
            exifInfo->orientation = EXIF_ORIENTATION_270;
            break;
        case 0:
        default:
            exifInfo->orientation = EXIF_ORIENTATION_UP;
            break;
    }

    exifInfo->maker_note_size = 0;

    /* Date Time */
    struct timeval rawtime;
    struct tm timeinfo;
    gettimeofday(&rawtime, NULL);
    localtime_r((time_t *)&rawtime.tv_sec, &timeinfo);
    strftime((char *)exifInfo->date_time, 20, "%Y:%m:%d %H:%M:%S", &timeinfo);
    snprintf((char *)exifInfo->sec_time, 5, "%04d", (int)(rawtime.tv_usec/1000));

    /*
     * vendorSpecific2[0] : info
     * vendorSpecific2[100] : 0:sirc 1:cml
     * vendorSpecific2[101] : cml exposure
     * vendorSpecific2[102] : cml iso(gain)
     * vendorSpecific2[103] : cml Bv
     */

    /* ISO Speed Rating */
#if 0 /* TODO: Must be same with the sensitivity in Result Metadata */
    exifInfo->iso_speed_rating = shot->udm.internal.vendorSpecific[1];
#else
    if (shot->dm.sensor.sensitivity < (uint32_t)m_staticInfo->sensitivityRange[MIN]) {
        exifInfo->iso_speed_rating = m_staticInfo->sensitivityRange[MIN];
    } else {
        exifInfo->iso_speed_rating = shot->dm.sensor.sensitivity;
    }
#endif

    exposureTimeCapture = m_configurations->getCaptureExposureTime();

    /* Exposure Program */
    if (exposureTimeCapture == 0) {
        exifInfo->exposure_program = EXIF_DEF_EXPOSURE_PROGRAM;
    } else {
        exifInfo->exposure_program = EXIF_DEF_EXPOSURE_MANUAL;
    }

    /* Exposure Time */
    exifInfo->exposure_time.num = 1;
    if (exposureTimeCapture <= CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
        /* HACK : Sometimes, F/W does NOT send the exposureTime */
        if (shot->dm.sensor.exposureTime != 0)
            exifInfo->exposure_time.den = (uint32_t)round((double)1e9 / shot->dm.sensor.exposureTime);
        else
            exifInfo->exposure_time.num = 0;
    } else {
        exifInfo->exposure_time.num = (uint32_t) (exposureTimeCapture / 1000);
        exifInfo->exposure_time.den = 1000;
    }

    /* Shutter Speed */
    if (exposureTimeCapture <= CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
        exifInfo->shutter_speed.num = (int32_t) (ROUND_OFF_HALF(((double) (shot->udm.internal.vendorSpecific[3] / 256.f) * EXIF_DEF_APEX_DEN), 0));
        exifInfo->shutter_speed.den = EXIF_DEF_APEX_DEN;
    } else {
        exifInfo->shutter_speed.num = (int32_t)(log2((double)exposureTimeCapture / 1000000) * -100);
        exifInfo->shutter_speed.den = 100;
    }

    /* Aperture */
    uint32_t aperture_num = 0;
    float aperture_f = 0.0f;

    if (m_staticInfo->availableApertureValues == NULL) {
        aperture_num = exifInfo->fnumber.num;
    } else {
        aperture_f = ((float) shot->dm.lens.aperture / 100);
        aperture_num = (uint32_t)(round(aperture_f * COMMON_DENOMINATOR));
        exifInfo->fnumber.num = aperture_num;
    }

    exifInfo->aperture.num = APEX_FNUM_TO_APERTURE((double) (aperture_num) / (double) (exifInfo->fnumber.den)) * COMMON_DENOMINATOR;
    exifInfo->aperture.den = COMMON_DENOMINATOR;

    /* Brightness */
    int temp = shot->udm.internal.vendorSpecific[2];
    if ((int) shot->udm.ae.vendorSpecific[102] < 0)
        temp = -temp;
    exifInfo->brightness.num = (int32_t) (ROUND_OFF_HALF((double)((temp * EXIF_DEF_APEX_DEN)/256.f), 0));
    if ((int) shot->udm.ae.vendorSpecific[102] < 0)
        exifInfo->brightness.num = -exifInfo->brightness.num;
    exifInfo->brightness.den = EXIF_DEF_APEX_DEN;

    CLOGD(" udm->internal.vendorSpecific2[100](%d)", shot->udm.internal.vendorSpecific[0]);
    CLOGD(" udm->internal.vendorSpecific2[101](%d)", shot->udm.internal.vendorSpecific[1]);
    CLOGD(" udm->internal.vendorSpecific2[102](%d)", shot->udm.internal.vendorSpecific[2]);
    CLOGD(" udm->internal.vendorSpecific2[103](%d)", shot->udm.internal.vendorSpecific[3]);

    CLOGD(" iso_speed_rating(%d)", exifInfo->iso_speed_rating);
    CLOGD(" exposure_time(%d/%d)", exifInfo->exposure_time.num, exifInfo->exposure_time.den);
    CLOGD(" shutter_speed(%d/%d)", exifInfo->shutter_speed.num, exifInfo->shutter_speed.den);
    CLOGD(" aperture     (%d/%d)", exifInfo->aperture.num, exifInfo->aperture.den);
    CLOGD(" brightness   (%d/%d)", exifInfo->brightness.num, exifInfo->brightness.den);

    /* Exposure Bias */
    exifInfo->exposure_bias.num = shot->ctl.aa.aeExpCompensation * (m_staticInfo->exposureCompensationStep * 10);
    exifInfo->exposure_bias.den = 10;

    /* Metering Mode */
#ifdef SAMSUNG_RTHDR
    enum camera2_wdr_mode wdr_mode;
    wdr_mode = shot->uctl.isModeUd.wdr_mode;
    if (wdr_mode == CAMERA_WDR_ON) {
        exifInfo->metering_mode = EXIF_METERING_PATTERN;
    } else
#endif
    {
        switch (shot->ctl.aa.aeMode) {
        case AA_AEMODE_CENTER:
                exifInfo->metering_mode = EXIF_METERING_CENTER;
                break;
        case AA_AEMODE_MATRIX:
                exifInfo->metering_mode = EXIF_METERING_AVERAGE;
                break;
        case AA_AEMODE_SPOT:
#ifdef SAMSUNG_CONTROL_METERING
        case AA_AEMODE_CENTER_TOUCH :
        case AA_AEMODE_SPOT_TOUCH :
        case AA_AEMODE_MATRIX_TOUCH :
 #endif
                exifInfo->metering_mode = EXIF_METERING_SPOT;
                break;
            default:
                exifInfo->metering_mode = EXIF_METERING_AVERAGE;
                break;
        }

#ifdef SAMSUNG_FOOD_MODE
        if(shot->ctl.aa.sceneMode == AA_SCENE_MODE_FOOD) {
            exifInfo->metering_mode = EXIF_METERING_AVERAGE;
        }
#endif
    }

    /* Flash Mode */
    if (shot->ctl.flash.flashMode == CAM2_FLASH_MODE_TORCH) {
        exifInfo->flash = 1;
    } else {
        exifInfo->flash = m_configurations->getModeValue(CONFIGURATION_MARKING_EXIF_FLASH);
    }

    /* White Balance */
    if (shot->ctl.aa.awbMode == AA_AWBMODE_WB_AUTO)
        exifInfo->white_balance = EXIF_WB_AUTO;
    else
        exifInfo->white_balance = EXIF_WB_MANUAL;

    /* Focal Length in 35mm length */
    exifInfo->focal_length_in_35mm_length = getFocalLengthIn35mmFilm();

    /* Scene Capture Type */
    switch (shot->ctl.aa.sceneMode) {
        case AA_SCENE_MODE_PORTRAIT:
        case AA_SCENE_MODE_FACE_PRIORITY:
            exifInfo->scene_capture_type = EXIF_SCENE_PORTRAIT;
            break;
        case AA_SCENE_MODE_LANDSCAPE:
            exifInfo->scene_capture_type = EXIF_SCENE_LANDSCAPE;
            break;
        case AA_SCENE_MODE_NIGHT:
            exifInfo->scene_capture_type = EXIF_SCENE_NIGHT;
            break;
        default:
            exifInfo->scene_capture_type = EXIF_SCENE_STANDARD;
            break;
    }

#ifdef SAMSUNG_TN_FEATURE
    switch (m_configurations->getModeValue(CONFIGURATION_SHOT_MODE)) {
        case SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_BEAUTY:
        case SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELFIE_FOCUS:
            exifInfo->scene_capture_type = EXIF_SCENE_PORTRAIT;
            break;
        default:
            break;
    }
#endif

    /* Image Unique ID */
#if defined(SAMSUNG_TN_FEATURE) && defined(SENSOR_FW_GET_FROM_FILE)
    char *front_fw = NULL;
    char *rear2_fw = NULL;
    char *savePtr;

    if (getCameraId() == CAMERA_ID_BACK){
        memset(exifInfo->unique_id, 0, sizeof(exifInfo->unique_id));
        strncpy((char *)exifInfo->unique_id,
                getSensorFWFromFile(m_staticInfo, m_cameraId), sizeof(exifInfo->unique_id) - 1);
    } else if (getCameraId() == CAMERA_ID_FRONT) {
        front_fw = strtok_r((char *)getSensorFWFromFile(m_staticInfo, m_cameraId), " ", &savePtr);
        strcpy((char *)exifInfo->unique_id, front_fw);
    }
#ifdef USE_DUAL_CAMERA
    else if (getCameraId() == CAMERA_ID_BACK_1) {
        rear2_fw = strtok_r((char *)getSensorFWFromFile(m_staticInfo, m_cameraId), " ", &savePtr);
        strcpy((char *)exifInfo->unique_id, rear2_fw);
    }
#endif /* USE_DUAL_CAMERA */
#endif

    /* GPS Coordinates */
    double gpsLatitude = shot->ctl.jpeg.gpsCoordinates[0];
    double gpsLongitude = shot->ctl.jpeg.gpsCoordinates[1];
    double gpsAltitude = shot->ctl.jpeg.gpsCoordinates[2];
    if (gpsLatitude != 0 && gpsLongitude != 0) {
        if (gpsLatitude > 0)
            strncpy((char *) exifInfo->gps_latitude_ref, "N", 2);
        else
            strncpy((char *) exifInfo->gps_latitude_ref, "S", 2);

        if (gpsLongitude > 0)
            strncpy((char *) exifInfo->gps_longitude_ref, "E", 2);
        else
            strncpy((char *) exifInfo->gps_longitude_ref, "W", 2);

        if (gpsAltitude > 0)
            exifInfo->gps_altitude_ref = 0;
        else
            exifInfo->gps_altitude_ref = 1;

        gpsLatitude = fabs(gpsLatitude);
        gpsLongitude = fabs(gpsLongitude);
        gpsAltitude = fabs(gpsAltitude);

        exifInfo->gps_latitude[0].num = (uint32_t) gpsLatitude;
        exifInfo->gps_latitude[0].den = 1;
        exifInfo->gps_latitude[1].num = (uint32_t)((gpsLatitude - exifInfo->gps_latitude[0].num) * 60);
        exifInfo->gps_latitude[1].den = 1;
        exifInfo->gps_latitude[2].num = (uint32_t)(round((((gpsLatitude - exifInfo->gps_latitude[0].num) * 60)
                        - exifInfo->gps_latitude[1].num) * 60));
        exifInfo->gps_latitude[2].den = 1;

        exifInfo->gps_longitude[0].num = (uint32_t)gpsLongitude;
        exifInfo->gps_longitude[0].den = 1;
        exifInfo->gps_longitude[1].num = (uint32_t)((gpsLongitude - exifInfo->gps_longitude[0].num) * 60);
        exifInfo->gps_longitude[1].den = 1;
        exifInfo->gps_longitude[2].num = (uint32_t)(round((((gpsLongitude - exifInfo->gps_longitude[0].num) * 60)
                        - exifInfo->gps_longitude[1].num) * 60));
        exifInfo->gps_longitude[2].den = 1;

        exifInfo->gps_altitude.num = (uint32_t)gpsAltitude;
        exifInfo->gps_altitude.den = 1;

        struct tm tm_data;
        long gpsTimestamp = (long) shot->ctl.jpeg.gpsTimestamp;
        gmtime_r(&gpsTimestamp, &tm_data);
        exifInfo->gps_timestamp[0].num = tm_data.tm_hour;
        exifInfo->gps_timestamp[0].den = 1;
        exifInfo->gps_timestamp[1].num = tm_data.tm_min;
        exifInfo->gps_timestamp[1].den = 1;
        exifInfo->gps_timestamp[2].num = tm_data.tm_sec;
        exifInfo->gps_timestamp[2].den = 1;
        snprintf((char*)exifInfo->gps_datestamp, sizeof(exifInfo->gps_datestamp),
                "%04d:%02d:%02d", tm_data.tm_year + 1900, tm_data.tm_mon + 1, tm_data.tm_mday);

        if (strlen((char *)shot->ctl.jpeg.gpsProcessingMethod) > 0) {
            size_t len = GPS_PROCESSING_METHOD_SIZE;
            memset(exifInfo->gps_processing_method, 0, sizeof(exifInfo->gps_processing_method));

            if (len > sizeof(exifInfo->gps_processing_method)) {
                len = sizeof(exifInfo->gps_processing_method);
            }
            strncpy((char *)exifInfo->gps_processing_method, (char *)shot->ctl.jpeg.gpsProcessingMethod, len);
        }

        exifInfo->enableGps = true;
    } else {
        exifInfo->enableGps = false;
    }

    /* Thumbnail Size */
    exifInfo->widthThumb = thumbnailRect->w;
    exifInfo->heightThumb = thumbnailRect->h;
}

int ExynosCameraParameters::getSensorControlDelay()
{
    int sensorRequestDelay = 0;
#ifdef SENSOR_REQUEST_DELAY
    if (m_configurations->getSamsungCamera() == true
        || m_configurations->getMode(CONFIGURATION_VISION_MODE) == true) {
        sensorRequestDelay = 0;
    } else {
        sensorRequestDelay = SENSOR_REQUEST_DELAY;
    }
#else
    android_printAssert(NULL, LOG_TAG, "SENSOR_REQUEST_DELAY is NOT defined.");
#endif
    CLOGV(" sensorRequestDelay %d", sensorRequestDelay);

    return sensorRequestDelay;
}

status_t ExynosCameraParameters::duplicateCtrlMetadata(void *buf)
{
    if (buf == NULL) {
        CLOGE("ERR: buf is NULL");
        return BAD_VALUE;
    }

    struct camera2_shot_ext *meta_shot_ext = (struct camera2_shot_ext *)buf;
    memcpy(&meta_shot_ext->shot.ctl, &m_metadata.shot.ctl, sizeof(struct camera2_ctl));

    setMetaVtMode(meta_shot_ext, (enum camera_vt_mode)m_configurations->getModeValue(CONFIGURATION_VT_MODE));
#ifdef SAMSUNG_RTHDR
    meta_shot_ext->shot.uctl.isModeUd.wdr_mode = getRTHdr();
#endif
#ifdef SAMSUNG_PAF
    meta_shot_ext->shot.uctl.isModeUd.paf_mode = getPaf();
#endif

#ifdef SUPPORT_DEPTH_MAP
    meta_shot_ext->shot.uctl.isModeUd.disparity_mode = m_metadata.shot.uctl.isModeUd.disparity_mode;
#endif

    return NO_ERROR;
}

void ExynosCameraParameters::setActiveZoomRatio(float zoomRatio)
{
    m_activeZoomRatio = zoomRatio;
}

float ExynosCameraParameters::getActiveZoomRatio(void)
{
    return m_activeZoomRatio;
}

void ExynosCameraParameters::setActiveZoomRect(ExynosRect zoomRect)
{
    m_activeZoomRect.x = zoomRect.x;
    m_activeZoomRect.y = zoomRect.y;
    m_activeZoomRect.w = zoomRect.w;
    m_activeZoomRect.h = zoomRect.h;
}

void ExynosCameraParameters::getActiveZoomRect(ExynosRect *zoomRect)
{
    zoomRect->x = m_activeZoomRect.x;
    zoomRect->y = m_activeZoomRect.y;
    zoomRect->w = m_activeZoomRect.w;
    zoomRect->h = m_activeZoomRect.h;
}

void ExynosCameraParameters::setActiveZoomMargin(int zoomMargin)
{
    m_activeZoomMargin = zoomMargin;
}

int ExynosCameraParameters::getActiveZoomMargin(void)
{
    return m_activeZoomMargin;
}

void ExynosCameraParameters::updatePreviewStatRoi(struct camera2_shot_ext *shot_ext, ExynosRect *bCropRect)
{
    float zoomRatio = m_configurations->getZoomRatio();
    float activeZoomRatio = getActiveZoomRatio();
    float statRoiZoomRatio = 1.0f;
    ExynosRect statRoi = {0, };

    if (m_scenario == SCENARIO_NORMAL
        || zoomRatio == activeZoomRatio) {
        shot_ext->shot.uctl.statsRoi[0] = bCropRect->x;
        shot_ext->shot.uctl.statsRoi[1] = bCropRect->y;
        shot_ext->shot.uctl.statsRoi[2] = bCropRect->w;
        shot_ext->shot.uctl.statsRoi[3] = bCropRect->h;
        return;
    }

    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM) {
        if (m_cameraId == CAMERA_ID_BACK_1) {
            /* for Tele View Angle */
            statRoiZoomRatio = (zoomRatio/2 < 1.0f) ? 1.0f : zoomRatio/2;
        } else {
            statRoiZoomRatio = zoomRatio;
        }
    } else {
        statRoiZoomRatio = zoomRatio;
    }

    statRoi.w = (int) ceil((float)(bCropRect->w * activeZoomRatio) / (float)statRoiZoomRatio);
    statRoi.h = (int) ceil((float)(bCropRect->h * activeZoomRatio) / (float)statRoiZoomRatio);
    statRoi.x = ALIGN_DOWN(((bCropRect->w - statRoi.w) >> 1), 2);
    statRoi.y = ALIGN_DOWN(((bCropRect->h - statRoi.h) >> 1), 2);

    CLOGV2("CameraId(%d), zoomRatio(%f->%f), bnsSize(%d,%d,%d,%d), statRoi(%d,%d,%d,%d)",
        m_cameraId,
        zoomRatio,
        statRoiZoomRatio,
        bCropRect->x,
        bCropRect->y,
        bCropRect->w,
        bCropRect->h,
        statRoi.x,
        statRoi.y,
        statRoi.w,
        statRoi.h);

    shot_ext->shot.uctl.statsRoi[0] = statRoi.x;
    shot_ext->shot.uctl.statsRoi[1] = statRoi.y;
    shot_ext->shot.uctl.statsRoi[2] = statRoi.w;
    shot_ext->shot.uctl.statsRoi[3] = statRoi.h;
}

status_t ExynosCameraParameters::m_vendorReInit(void)
{
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    m_captureExtCropROI = {0, };
#endif
    m_depthMapW = 0;
    m_depthMapH = 0;

#ifdef LLS_CAPTURE
    m_LLSValue = 0;
    memset(m_needLLS_history, 0x00, sizeof(int) * LLS_HISTORY_COUNT);
#endif
#ifdef SAMSUNG_DOF
    m_setLensPos(0);
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    m_aeState = 0;
    m_sceneDetectIndex = -1;
#endif

    return NO_ERROR;
}

void ExynosCameraParameters::getVendorRatioCropSizeForVRA(__unused ExynosRect *ratioCropSize)
{
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM
        && m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
        ExynosRect inputRect;
        ExynosRect fusionSrcRect;
        ExynosRect fusionDstRect;
        int cropW = 0, cropH = 0;
        int cropShiftX = 0, cropShiftY = 0;
        int outWidth = 0, outHeight = 0;
        int dispCamId = CAMERA_ID_BACK;
        int dispCamType = m_configurations->getModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE);

        inputRect.x = ratioCropSize->x;
        inputRect.y = ratioCropSize->y;
        inputRect.w = ratioCropSize->w;
        inputRect.h = ratioCropSize->h;

        if (dispCamType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
            dispCamId = CAMERA_ID_BACK_1;
        }

        if (m_cameraId == dispCamId) {
            m_configurations->getSize(CONFIGURATION_PREVIEW_SIZE, (uint32_t *)&outWidth, (uint32_t *)&outHeight);

            /* Don't care outWidth and outHeight */
            getFusionSize(outWidth, outHeight, &fusionSrcRect, &fusionDstRect);
            cropW = fusionDstRect.w;
            cropH = fusionDstRect.h;

            cropShiftX = m_configurations->getModeValue(CONFIGURATION_DUAL_PREVIEW_SHIFT_X);
            cropShiftY = m_configurations->getModeValue(CONFIGURATION_DUAL_PREVIEW_SHIFT_Y);

            ratioCropSize->x = -cropShiftX;
            ratioCropSize->y = -cropShiftY;
            ratioCropSize->w = cropW;
            ratioCropSize->h = cropH;

            /* x - y */
            if (ratioCropSize->x < 0) {
                ratioCropSize->x = 0;
            }

            if (ratioCropSize->y < 0) {
                ratioCropSize->y = 0;
            }

            /* w - h */
            if (ratioCropSize->w > inputRect.w) {
                ratioCropSize->x = 0;
                ratioCropSize->w = inputRect.w;
            } else if (ratioCropSize->x + ratioCropSize->w > inputRect.w) {
                ratioCropSize->x = ALIGN_DOWN((inputRect.w - ratioCropSize->w)/2, 2);
            }

            if (ratioCropSize->h > inputRect.h) {
                ratioCropSize->y = 0;
                ratioCropSize->h = inputRect.h;
            } else if (ratioCropSize->y + ratioCropSize->h > inputRect.h) {
                ratioCropSize->y = ALIGN_DOWN((inputRect.h - ratioCropSize->h)/2, 2);
            }

            CLOGV2("CameraId(%d),Shift(%dx%d)(%dx%d), ratioCropSize(%d,%d,%d,%d)",
                m_cameraId,
                cropShiftX, cropShiftY, cropW, cropH,
                ratioCropSize->x, ratioCropSize->y, ratioCropSize->w, ratioCropSize->h);
        }
    }
#endif
}

void ExynosCameraParameters::getVendorRatioCropSize(__unused ExynosRect *ratioCropSize, __unused ExynosRect *mcscSize, __unused int portIndex, __unused int isReprocessing
#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_ZOOM_SUPPORTED)
                                                                , ExynosRect *sensorSize, ExynosRect *mcscInputSize
#endif
) {
#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_ZOOM_SUPPORTED)
    if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
        int index = FIMC_IS_VIDEO_M0P_NUM + portIndex;
        if (index == FIMC_IS_VIDEO_M0P_NUM) {
            getHiFiVideoScalerControlInfo(FIMC_IS_VIDEO_M0P_NUM, mcscSize, mcscInputSize, mcscSize);
        } else {
            getHiFiVideoScalerControlInfo(index, sensorSize, mcscInputSize, ratioCropSize);
        }
        return;
    } else {
        mcscSize->x = 0;
        mcscSize->y = 0;
    }
#endif

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    if (isReprocessing) {
        if (useCaptureExtCropROI()) {
            int yuvStallPort = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT);

            /* get catpureCropROI Rect */
            ratioCropSize->x = m_captureExtCropROI.x;
            ratioCropSize->y = m_captureExtCropROI.y;
            ratioCropSize->w = m_captureExtCropROI.w;
            ratioCropSize->h = m_captureExtCropROI.h;

#ifdef SAMSUNG_HIFI_CAPTURE
            if ((yuvStallPort == portIndex)
                && (m_configurations->getDynamicMode(DYNAMIC_HIFI_CAPTURE_MODE) == true)
                && (getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON)
            ) {
                mcscSize->w = ALIGN_UP(ratioCropSize->w, 16);
                mcscSize->h = ALIGN_UP(ratioCropSize->h, 16);
                CLOGD2("[LLS_MBR_%d] mcscSize[%d].w: %d, mcscSize[%d].h: %d",
                    m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE),
                    portIndex, mcscSize->w, portIndex, mcscSize->h);
            }
#endif
        }
    }
#endif
}

#ifdef SAMSUNG_TN_FEATURE
void ExynosCameraParameters::setImageUniqueId(char *uniqueId)
{
    memcpy(m_cameraInfo.imageUniqueId, uniqueId, sizeof(m_cameraInfo.imageUniqueId));
}
#endif

#ifdef SAMSUNG_DOF
void ExynosCameraParameters::m_setLensPos(int pos)
{
    CLOGD("[DOF]position: %d", pos);
    setMetaCtlLensPos(&m_metadata, pos);
}
#endif

#ifdef SAMSUNG_LLS_DEBLUR
void ExynosCameraParameters::setLLSdebugInfo(unsigned char *data, unsigned int size)
{
    char sizeInfo[2] = {(unsigned char)((size >> 8) & 0xFF), (unsigned char)(size & 0xFF)};

    memset(m_staticInfo->lls_exif_info.lls_exif, 0, LLS_EXIF_SIZE);
    memcpy(m_staticInfo->lls_exif_info.lls_exif, LLS_EXIF_TAG, sizeof(LLS_EXIF_TAG));
    memcpy(m_staticInfo->lls_exif_info.lls_exif + sizeof(LLS_EXIF_TAG) - 1, sizeInfo, sizeof(sizeInfo));
    memcpy(m_staticInfo->lls_exif_info.lls_exif + sizeof(LLS_EXIF_TAG) + sizeof(sizeInfo)- 1, data, size);
}
#endif

#ifdef SAMSUNG_MFHDR_CAPTURE
void ExynosCameraParameters::setMFHDRdebugInfo(unsigned char *data, unsigned int size)
{
    char sizeInfo[2] = {(unsigned char)((size >> 8) & 0xFF), (unsigned char)(size & 0xFF)};

    memset(m_staticInfo->lls_exif_info.lls_exif, 0, LLS_EXIF_SIZE);
    memcpy(m_staticInfo->lls_exif_info.lls_exif, MFHDR_EXIF_TAG, sizeof(MFHDR_EXIF_TAG));
    memcpy(m_staticInfo->lls_exif_info.lls_exif + sizeof(MFHDR_EXIF_TAG) - 1, sizeInfo, sizeof(sizeInfo));
    memcpy(m_staticInfo->lls_exif_info.lls_exif + sizeof(MFHDR_EXIF_TAG) + sizeof(sizeInfo)- 1, data, size);
}
#endif

#ifdef SAMSUNG_LLHDR_CAPTURE
void ExynosCameraParameters::setLLHDRdebugInfo(unsigned char *data, unsigned int size)
{
    char sizeInfo[2] = {(unsigned char)((size >> 8) & 0xFF), (unsigned char)(size & 0xFF)};

    memset(m_staticInfo->lls_exif_info.lls_exif, 0, LLS_EXIF_SIZE);
    memcpy(m_staticInfo->lls_exif_info.lls_exif, LLHDR_EXIF_TAG, sizeof(LLHDR_EXIF_TAG));
    memcpy(m_staticInfo->lls_exif_info.lls_exif + sizeof(LLHDR_EXIF_TAG) - 1, sizeInfo, sizeof(sizeInfo));
    memcpy(m_staticInfo->lls_exif_info.lls_exif + sizeof(LLHDR_EXIF_TAG) + sizeof(sizeInfo)- 1, data, size);
}
#endif

#ifdef SUPPORT_ZSL_MULTIFRAME
void ExynosCameraParameters::checkZslMultiframeMode(void)
{
    ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();
    uint32_t lls = getLLSValue();

    m_configurations->setMode(CONFIGURATION_ZSL_MULTIFRAME_MODE, false);

    if (!(m_cameraId == CAMERA_ID_BACK || m_cameraId == CAMERA_ID_BACK_1)) {
        return;
    }

    if (!(lls >= LLS_LEVEL_MULTI_MERGE_ZSL_2
        && lls <= LLS_LEVEL_MULTI_MERGE_ZSL_5)) {
        return;
    }

    if (m_configurations->getCaptureExposureTime() > 0) {
        CLOGD("zsl multi-frame mode off for manual shutter speed");
        return;
    }

    bool recordingEnabled = m_configurations->getMode(CONFIGURATION_RECORDING_MODE);
    int shotMode = m_configurations->getModeValue(CONFIGURATION_SHOT_MODE);

    if ((recordingEnabled == true)
        || (shotMode > SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_AUTO
            && shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_NIGHT)) {
        if (recordingEnabled == true
            || (shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_BEAUTY
                && shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELFIE_FOCUS
                && shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELECTIVE_FOCUS
                && shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_PRO)) {
            CLOGD("zsl multi-frame mode off for special shot %d", shotMode);
            return;
        }
    }

#ifdef SR_CAPTURE
    if (getSROn() == true) {
        CLOGD("zsl multi-frame mode off (SR mode)");
        return;
    }
#endif

    if (m_configurations->getModeValue(CONFIGURATION_SERIES_SHOT_MODE) == SERIES_SHOT_MODE_NONE && m_flashMgr->getNeedCaptureFlash()
#ifdef FLASHED_LLS_CAPTURE
    && !getFlashedLLSCapture()
#endif
        ) {
        CLOGD("zsl multi-frame mode off for flash single capture");
        return;
    }

    m_configurations->setMode(CONFIGURATION_ZSL_MULTIFRAME_MODE, true);
    CLOGD("zsl multi-frame mode on");
}
#endif

#ifdef LLS_CAPTURE
void ExynosCameraParameters::setLLSValue(struct camera2_shot_ext *shot)
{
    int lls_value = 0;

    if (shot == NULL) {
        return;
    }

    m_needLLS_history[3] = m_needLLS_history[2];
    m_needLLS_history[2] = m_needLLS_history[1];
    m_needLLS_history[1] = getLLS(shot);

    if (m_needLLS_history[1] == m_needLLS_history[2]
            && m_needLLS_history[1] == m_needLLS_history[3]) {
        lls_value = m_needLLS_history[0] = m_needLLS_history[1];
    } else {
        lls_value = m_needLLS_history[0];
    }

    if (lls_value != getLLSValue()) {
        m_LLSValue = lls_value;
        CLOGD("[%d[%d][%d][%d] LLS_value(%d)",
                m_needLLS_history[0], m_needLLS_history[1], m_needLLS_history[2], m_needLLS_history[3],
                m_LLSValue);
    }
}

int ExynosCameraParameters::getLLSValue(void)
{
    return m_LLSValue;
}
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
void ExynosCameraParameters::storeAeState(int aeState)
{
    m_aeState = aeState;
}

int ExynosCameraParameters::getAeState(void)
{
    return m_aeState;
}

void ExynosCameraParameters::storeSceneDetectIndex(int scene_index)
{
    m_sceneDetectIndex = scene_index;
}

int ExynosCameraParameters::getSceneDetectIndex(void)
{
    return m_sceneDetectIndex;
}
#endif

#ifdef OIS_CAPTURE
void ExynosCameraParameters::checkOISCaptureMode(int multiCaptureMode)
{
    ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();
#ifdef LLS_CAPTURE
    uint32_t lls = getLLSValue();
#else
    uint32_t lls = 0;
#endif
    if (m_configurations->getSamsungCamera() == false) {
        CLOGD("zsl-like capture mode off for 3rd party app");
        m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, false);
        return;
    }

    if (m_configurations->getMode(CONFIGURATION_MANUAL_AE_CONTROL_MODE) == true) {
        CLOGD("zsl-like capture mode off for manual shutter speed");
        m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, false);
        return;
    }

    bool recordingEnabled = m_configurations->getMode(CONFIGURATION_RECORDING_MODE);
    int shotMode = m_configurations->getModeValue(CONFIGURATION_SHOT_MODE);

    if ((recordingEnabled == true)
         || (shotMode > SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_AUTO
             && shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_NIGHT
             && shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_FOOD
             && shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_STICKER)) {
        if (recordingEnabled == true
            || (shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_BEAUTY
                && shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELFIE_FOCUS
                && shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELECTIVE_FOCUS
                && shotMode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_PRO)) {
            m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, false);
            CLOGD(" zsl-like capture mode off for special shot");
            return;
        }
    }

    if (multiCaptureMode == MULTI_CAPTURE_MODE_NONE && m_flashMgr->getNeedCaptureFlash()) {
        m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, false);
        CLOGD(" zsl-like capture mode off for flash single capture");
        return;
    }

    if (multiCaptureMode != MULTI_CAPTURE_MODE_NONE && lls > 0
#ifdef SUPPORT_ZSL_MULTIFRAME
        && (!((m_cameraId == CAMERA_ID_BACK || m_cameraId == CAMERA_ID_BACK_1)
            && lls >= LLS_LEVEL_MULTI_MERGE_ZSL_2 && lls <= LLS_LEVEL_MULTI_MERGE_ZSL_5))
#endif
    ) {
        CLOGD(" zsl-like multi capture mode on, lls(%d)", lls);
        m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, true);
        return;
    }

#ifdef LLS_CAPTURE
    bool flagLLSOn = m_configurations->getMode(CONFIGURATION_LIGHT_CONDITION_ENABLE_MODE);
    int flashMode = m_configurations->getModeValue(CONFIGURATION_FLASH_MODE);

    CLOGD("Low Light value(%d), flagLLSOn(%d), FlashMode(%d)",
            lls, flagLLSOn, flashMode);

    if (flagLLSOn) {
        switch (flashMode) {
        case FLASH_MODE_AUTO:
        case FLASH_MODE_OFF:
            if (lls == LLS_LEVEL_ZSL_LIKE || lls == LLS_LEVEL_LOW || lls == LLS_LEVEL_FLASH
#ifdef SAMSUNG_LLS_DEBLUR
                    || lls == LLS_LEVEL_ZSL_LIKE1
                    || (lls >= LLS_LEVEL_MULTI_MERGE_2 && lls <= LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_5)
#ifdef SAMSUNG_MFHDR_CAPTURE
                    || (lls >= LLS_LEVEL_MULTI_MFHDR_2 && lls <= LLS_LEVEL_MULTI_MFHDR_5)
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
                    || (lls >= LLS_LEVEL_MULTI_LLHDR_LOW_2 && lls <= LLS_LEVEL_MULTI_LLHDR_5)
#endif
#ifdef SUPPORT_ZSL_MULTIFRAME
                    || (lls >= LLS_LEVEL_MULTI_MERGE_LOW_2 && lls <= LLS_LEVEL_MULTI_MERGE_LOW_5)
#endif
#endif
               ) {
                m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, true);
            }
            break;
        case FLASH_MODE_ON:
        case FLASH_MODE_TORCH:
        case FLASH_MODE_RED_EYE:
            if (lls == LLS_LEVEL_FLASH) {
                m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, true);
            }
            break;
        default:
            m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, false);
            break;
        }
    } else {
        switch (flashMode) {
        case FLASH_MODE_AUTO:
            if ((lls == LLS_LEVEL_ZSL_LIKE || lls == LLS_LEVEL_ZSL_LIKE1)
                && m_flashMgr->getNeedFlash() == false) {
                m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, true);
            }
            break;
        case FLASH_MODE_OFF:
            if (lls == LLS_LEVEL_ZSL_LIKE || lls == LLS_LEVEL_ZSL_LIKE1) {
                m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, true);
            }
            break;
        default:
            m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, false);
            break;
        }
    }
#endif
}
#endif

#ifdef SAMSUNG_LLS_DEBLUR
void ExynosCameraParameters::checkLDCaptureMode(bool skipLDCapture)
{
    int ldCaptureMode = MULTI_SHOT_MODE_NONE;
    int ldCaptureCount = 0;
#ifdef LLS_CAPTURE
    int32_t lls = getLLSValue();
#endif
    float zoomRatio = m_configurations->getZoomRatio();
    int copyStepCount = 0;

#ifdef FAST_SHUTTER_NOTIFY
    m_configurations->setMode(CONFIGURATION_FAST_SHUTTER_MODE, false);
#endif

    if (m_configurations->getSamsungCamera() == false || skipLDCapture == true) {
        CLOGD("LLS Deblur capture mode off for 3rd party or skipLDCapture is %d", skipLDCapture);
        ldCaptureMode = MULTI_SHOT_MODE_NONE;
        ldCaptureCount = 0;
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_MODE, ldCaptureMode);
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_COUNT, ldCaptureCount);
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_LLS_VALUE, LLS_LEVEL_ZSL);
        m_configurations->setModeValue(CONFIGURATION_EXIF_CAPTURE_STEP_COUNT, copyStepCount);
        return;
    }

#ifdef SUPPORT_ZSL_MULTIFRAME
    checkZslMultiframeMode();
#endif

    if (m_cameraId == CAMERA_ID_FRONT
        && (m_configurations->getMode(CONFIGURATION_RECORDING_MODE) == true
            || m_configurations->getMode(CONFIGURATION_PIP_MODE) == true
            || (m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) > SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_AUTO
                && m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_BEAUTY
                && m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELFIE_FOCUS
                && m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_NIGHT))) {
        CLOGD("LLS Deblur capture mode off for special shot");
        ldCaptureMode = MULTI_SHOT_MODE_NONE;
        ldCaptureCount = 0;
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_MODE, ldCaptureMode);
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_COUNT, ldCaptureCount);
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_LLS_VALUE, LLS_LEVEL_ZSL);
        m_configurations->setModeValue(CONFIGURATION_EXIF_CAPTURE_STEP_COUNT, copyStepCount);
        return;
    }

#if 0 /* REMOVE - IQ TEAM GUIDE */
    if (m_cameraId == CAMERA_ID_BACK && zoomRatio >= 3.0f) {
        CLOGD("LLS Deblur capture mode off. zoomRatio(%f) LLS Value(%d)",
                zoomRatio, m_configurations->getModeValue(CONFIGURATION_LLS_VALUE));
        ldCaptureMode = MULTI_SHOT_MODE_NONE;
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_COUNT, 0);
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_LLS_VALUE, LLS_LEVEL_ZSL);
        return;
    }
#endif

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    if (m_configurations->getMode(CONFIGURATION_FUSION_CAPTURE_MODE)) {
        ldCaptureMode = MULTI_SHOT_MODE_NONE;
        ldCaptureCount = 0;
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_MODE, ldCaptureMode);
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_COUNT, ldCaptureCount);
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_LLS_VALUE, LLS_LEVEL_ZSL);
        m_configurations->setModeValue(CONFIGURATION_EXIF_CAPTURE_STEP_COUNT, copyStepCount);
        return;
    }
#endif

#ifdef SAMSUNG_HIFI_CAPTURE
    if (m_configurations->getSamsungCamera() == true
        && m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_AUTO
        && m_configurations->getMode(CONFIGURATION_OIS_CAPTURE_MODE) == false
        && zoomRatio >= 4.0f
        && lls == LLS_LEVEL_ZSL) {
        CLOGD("enable SR Capture");
        ldCaptureMode = MULTI_SHOT_MODE_SR;
        ldCaptureCount = LD_CAPTURE_COUNT_MULTI4;
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_COUNT, ldCaptureCount);
        m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_LLS_VALUE, lls);
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_MODE, ldCaptureMode);
        m_configurations->setModeValue(CONFIGURATION_EXIF_CAPTURE_STEP_COUNT, copyStepCount);
        return;
    }
#endif

#ifdef LLS_CAPTURE
    if (m_cameraId == CAMERA_ID_FRONT
#ifdef SUPPORT_ZSL_MULTIFRAME
        || m_configurations->getMode(CONFIGURATION_ZSL_MULTIFRAME_MODE) == true
#endif
        || m_configurations->getMode(CONFIGURATION_OIS_CAPTURE_MODE) == true) {
        if (lls == LLS_LEVEL_MULTI_MERGE_2
            || lls == LLS_LEVEL_MULTI_PICK_2
#ifdef SUPPORT_ZSL_MULTIFRAME
            || lls == LLS_LEVEL_MULTI_MERGE_LOW_2
            || lls == LLS_LEVEL_MULTI_MERGE_ZSL_2
#endif
            || lls == LLS_LEVEL_MULTI_MERGE_INDICATOR_2
            || lls == LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_2) {
            ldCaptureMode = MULTI_SHOT_MODE_MULTI1;
            ldCaptureCount = LD_CAPTURE_COUNT_MULTI1;
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        } else if (lls == LLS_LEVEL_MULTI_MERGE_3
#ifdef SUPPORT_ZSL_MULTIFRAME
                   || lls == LLS_LEVEL_MULTI_MERGE_LOW_3
                   || lls == LLS_LEVEL_MULTI_MERGE_ZSL_3
#endif
                   || lls == LLS_LEVEL_MULTI_PICK_3
                   || lls == LLS_LEVEL_MULTI_MERGE_INDICATOR_3
                   || lls == LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_3) {
            ldCaptureMode = MULTI_SHOT_MODE_MULTI2;
            ldCaptureCount = LD_CAPTURE_COUNT_MULTI2;
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        } else if (lls == LLS_LEVEL_MULTI_MERGE_4
                   || lls == LLS_LEVEL_MULTI_PICK_4
#ifdef SUPPORT_ZSL_MULTIFRAME
                   || lls == LLS_LEVEL_MULTI_MERGE_LOW_4
                   || lls == LLS_LEVEL_MULTI_MERGE_ZSL_4
#endif
                   || lls == LLS_LEVEL_MULTI_MERGE_INDICATOR_4
                   || lls == LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_4) {
            ldCaptureMode = MULTI_SHOT_MODE_MULTI3;
            ldCaptureCount = LD_CAPTURE_COUNT_MULTI3;
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        } else if (lls == LLS_LEVEL_MULTI_MERGE_5
                   || lls == LLS_LEVEL_MULTI_PICK_5
#ifdef SUPPORT_ZSL_MULTIFRAME
                   || lls == LLS_LEVEL_MULTI_MERGE_LOW_5
                   || lls == LLS_LEVEL_MULTI_MERGE_ZSL_5
#endif
                   || lls == LLS_LEVEL_MULTI_MERGE_INDICATOR_5
                   || lls == LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_5) {
            ldCaptureMode = MULTI_SHOT_MODE_MULTI4;
            ldCaptureCount = LD_CAPTURE_COUNT_MULTI4;
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        }
#ifdef SAMSUNG_MFHDR_CAPTURE
        else if (lls == LLS_LEVEL_MULTI_MFHDR_2) {
            ldCaptureMode = MULTI_SHOT_MODE_MF_HDR;
            ldCaptureCount = LD_CAPTURE_COUNT_MULTI1;
            copyStepCount = MFHDR_CAPTURE_COPY_EXIF_COUNT;
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        } else if (lls == LLS_LEVEL_MULTI_MFHDR_3) {
            ldCaptureMode = MULTI_SHOT_MODE_MF_HDR;
            ldCaptureCount = LD_CAPTURE_COUNT_MULTI2;
            copyStepCount = MFHDR_CAPTURE_COPY_EXIF_COUNT;
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        } else if (lls == LLS_LEVEL_MULTI_MFHDR_4) {
            ldCaptureMode = MULTI_SHOT_MODE_MF_HDR;
            ldCaptureCount = LD_CAPTURE_COUNT_MULTI3;
            copyStepCount = MFHDR_CAPTURE_COPY_EXIF_COUNT;
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        } else if (lls == LLS_LEVEL_MULTI_MFHDR_5) {
            ldCaptureMode = MULTI_SHOT_MODE_MF_HDR;
            ldCaptureCount = LD_CAPTURE_COUNT_MULTI4;
            copyStepCount = MFHDR_CAPTURE_COPY_EXIF_COUNT;
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        }
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
        else if (lls == LLS_LEVEL_MULTI_LLHDR_LOW_2 || lls == LLS_LEVEL_MULTI_LLHDR_2) {
            ldCaptureMode = MULTI_SHOT_MODE_LL_HDR;
            ldCaptureCount = LD_CAPTURE_COUNT_MULTI1;
            copyStepCount = LLHDR_CAPTURE_COPY_EXIF_COUNT;
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        } else if (lls == LLS_LEVEL_MULTI_LLHDR_LOW_3 || lls == LLS_LEVEL_MULTI_LLHDR_3) {
            ldCaptureMode = MULTI_SHOT_MODE_LL_HDR;
            ldCaptureCount = LD_CAPTURE_COUNT_MULTI2;
            copyStepCount = LLHDR_CAPTURE_COPY_EXIF_COUNT;
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        } else if (lls == LLS_LEVEL_MULTI_LLHDR_LOW_4 || lls == LLS_LEVEL_MULTI_LLHDR_4) {
            ldCaptureMode = MULTI_SHOT_MODE_LL_HDR;
            ldCaptureCount = LD_CAPTURE_COUNT_MULTI3;
            copyStepCount = LLHDR_CAPTURE_COPY_EXIF_COUNT;
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        } else if (lls == LLS_LEVEL_MULTI_LLHDR_LOW_5 || lls == LLS_LEVEL_MULTI_LLHDR_5) {
            ldCaptureMode = MULTI_SHOT_MODE_LL_HDR;
            ldCaptureCount = LD_CAPTURE_COUNT_MULTI4;
            copyStepCount = LLHDR_CAPTURE_COPY_EXIF_COUNT;
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        }
#endif
        else {
            ldCaptureMode = MULTI_SHOT_MODE_NONE;
            m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_COUNT, 0);
        }
    }

#if 0 /* REMOVE : IQ TEAM */
    if (ldCaptureMode == MULTI_SHOT_MODE_MF_HDR) {
        ExynosCameraActivityAutofocus *autoFocusMgr = m_activityControl->getAutoFocusMgr();
        enum aa_afstate afState = AA_AFSTATE_INACTIVE;

        if (autoFocusMgr != NULL) {
            afState = autoFocusMgr->getAfState();

            if ((afState != AA_AFSTATE_PASSIVE_FOCUSED) && (afState != AA_AFSTATE_FOCUSED_LOCKED)) {
                ldCaptureMode = MULTI_SHOT_MODE_MULTI2;
                ldCaptureCount = LD_CAPTURE_COUNT_MULTI2;
                lls = LLS_LEVEL_MULTI_MERGE_ZSL_3;

                CLOGD(" change to LDCaptureMode(%d) and CaptureCount(%d). afState(%d)",
                        ldCaptureMode, ldCaptureCount, afState);
            }
        } else {
            CLOGD("autoFocusMgr is NULL");
        }
    }
#endif

    m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_MODE, ldCaptureMode);
    m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_COUNT, ldCaptureCount);
    m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_LLS_VALUE, lls);
    m_configurations->setModeValue(CONFIGURATION_EXIF_CAPTURE_STEP_COUNT, copyStepCount);
#endif

#ifdef SET_LLS_CAPTURE_SETFILE
    if ((ldCaptureMode > MULTI_SHOT_MODE_NONE && ldCaptureMode <= MULTI_SHOT_MODE_MULTI4)
#ifdef SAMSUNG_LLHDR_CAPTURE
        || ldCaptureMode == MULTI_SHOT_MODE_LL_HDR
#endif
        ) {
#ifdef LLS_CAPTURE
        CLOGD("set LLS Capture mode(%d) on. lls value(%d)",
                ldCaptureMode, getLLSValue());
#else
        CLOGD("set LLS Capture mode on.");
#endif
        m_configurations->setMode(CONFIGURATION_LLS_CAPTURE_MODE, true);
    }
#endif

    if (m_cameraId == CAMERA_ID_BACK) {
        m_configurations->setMode(CONFIGURATION_FAST_SHUTTER_MODE, true);
        CLOGD("set fast shutter mode on.");
    }
}
#endif

#ifdef SAMSUNG_PAF
enum camera2_paf_mode ExynosCameraParameters::getPaf(void)
{
    enum camera2_paf_mode mode = CAMERA_PAF_OFF;

    getMetaCtlPaf(&m_metadata, &mode);

    return mode;
}
#endif

#ifdef SAMSUNG_RTHDR
enum camera2_wdr_mode ExynosCameraParameters::getRTHdr(void)
{
    enum camera2_wdr_mode mode = CAMERA_WDR_OFF;

    getMetaCtlRTHdr(&m_metadata, &mode);

    return mode;
}

void ExynosCameraParameters::setRTHdr(enum camera2_wdr_mode rtHdrMode)
{
    setMetaCtlRTHdr(&m_metadata, rtHdrMode);
}
#endif

#ifdef SAMSUNG_HRM
void ExynosCameraParameters::m_setHRM(int ir_data, int flicker_data, int status)
{
    setMetaCtlHRM(&m_metadata, ir_data, flicker_data, status);
    m_configurations->setModeValue(CONFIGURATION_FLICKER_DATA, flicker_data);
}
#endif

#ifdef SAMSUNG_LIGHT_IR
void ExynosCameraParameters::m_setLight_IR(SensorListenerEvent_t data)
{
    setMetaCtlLight_IR(&m_metadata, data);
}
#endif

#ifdef SAMSUNG_GYRO
void ExynosCameraParameters::m_setGyro(SensorListenerEvent_t data)
{
    setMetaCtlGyro(&m_metadata, data);
    memcpy(&m_gyroListenerData, &data, sizeof(SensorListenerEvent_t));
}
SensorListenerEvent_t *ExynosCameraParameters::getGyroData(void)
{
    return &m_gyroListenerData;
}
#endif

#ifdef SAMSUNG_ACCELEROMETER
void ExynosCameraParameters::m_setAccelerometer(SensorListenerEvent_t data)
{
    setMetaCtlAcceleration(&m_metadata, data);

}
#endif

#ifdef SAMSUNG_PROX_FLICKER
void ExynosCameraParameters::setProxFlicker(SensorListenerEvent_t data)
{
    setMetaCtlProxFlicker(&m_metadata, data);
}
#endif

#ifdef SAMSUNG_FACTORY_DRAM_TEST
int ExynosCameraParameters::getFactoryDramTestCount(void)
{
    return m_staticInfo->factoryDramTestCount;
}
#endif

#ifdef SAMSUNG_HYPERLAPSE
void ExynosCameraParameters::getHyperlapseYuvSize(int w, int h, int fps, int *newW, int *newH)
{
    int *videoSizeList = NULL;
    int videoSizeListMax = m_staticInfo->availableVideoListMax;

    if (w < 0 || h < 0) {
        return;
    }

    for (int i = 0; i < videoSizeListMax; i++) {
        videoSizeList = m_staticInfo->availableVideoList[i];
        if (w == videoSizeList[0] && h == videoSizeList[1] && fps == (videoSizeList[3] / 1000)) {
            *newW = videoSizeList[4];
            *newH = videoSizeList[5];
            break;
        }
    }
}

void ExynosCameraParameters::getHyperlapseAdjustYuvSize(int *width, int *height, int fps)
{
    int *videoSizeList = NULL;
    int videoSizeListMax = m_staticInfo->availableVideoListMax;

    for (int i = 0; i < videoSizeListMax; i++) {
        videoSizeList = m_staticInfo->availableVideoList[i];
        if (*width == videoSizeList[4] && *height == videoSizeList[5] && fps == (videoSizeList[3] / 1000)) {
            *width = videoSizeList[0];
            *height = videoSizeList[1];
            break;
        }
    }
}
#endif

#ifdef SAMSUNG_STR_CAPTURE
void ExynosCameraParameters::checkSTRCaptureMode(bool hasCaptureStream)
{
    if (getCameraId() != CAMERA_ID_FRONT
        || m_configurations->getZoomRatio() != 1.0f
        || m_configurations->getMode(CONFIGURATION_RECORDING_MODE) == true
        || m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_STICKER
        || hasCaptureStream == false) {
        m_configurations->setMode(CONFIGURATION_STR_CAPTURE_MODE, false);
    } else if (m_configurations->getSamsungCamera() == true) {
        if (m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_AUTO
            || m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_BEAUTY
            || m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELFIE_FOCUS) {
            m_configurations->setMode(CONFIGURATION_STR_CAPTURE_MODE, true);
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        } else {
            m_configurations->setMode(CONFIGURATION_STR_CAPTURE_MODE, false);
        }
    } else { // default on for 3rd party app
        m_configurations->setMode(CONFIGURATION_STR_CAPTURE_MODE, true);
        m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
    }

    CLOGD("getSTRCaptureEnable %d",
            m_configurations->getMode(CONFIGURATION_STR_CAPTURE_MODE));

    return;
}

void ExynosCameraParameters::setSTRdebugInfo(unsigned char *data, unsigned int size)
{
    char sizeInfo[2] = {(unsigned char)((size >> 8) & 0xFF), (unsigned char)(size & 0xFF)};

    memset(m_staticInfo->str_exif_info.str_exif, 0, STR_EXIF_SIZE);
    memcpy(m_staticInfo->str_exif_info.str_exif, STR_EXIF_TAG, sizeof(STR_EXIF_TAG));
    memcpy(m_staticInfo->str_exif_info.str_exif + sizeof(STR_EXIF_TAG) - 1, sizeInfo, sizeof(sizeInfo));
    memcpy(m_staticInfo->str_exif_info.str_exif + sizeof(STR_EXIF_TAG) + sizeof(sizeInfo) - 1, data, size);
}
#endif

#ifdef SAMSUNG_VIDEO_BEAUTY_SNAPSHOT
void ExynosCameraParameters::checkBeautyCaptureMode(bool hasCaptureStream)
{
    if (hasCaptureStream == false) {
        m_configurations->setMode(CONFIGURATION_VIDEO_BEAUTY_SNAPSHOT_MODE, false);
    } else {
        if (m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE)) {
            m_configurations->setMode(CONFIGURATION_VIDEO_BEAUTY_SNAPSHOT_MODE, true);
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        } else {
            m_configurations->setMode(CONFIGURATION_VIDEO_BEAUTY_SNAPSHOT_MODE, false);
        }
    }

    CLOGD("getBeautyCaptureEnable %d",
            m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_SNAPSHOT_MODE));
}
#endif

#ifdef SAMSUNG_STR_PREVIEW
bool ExynosCameraParameters::getSTRPreviewEnable(void)
{
    int shotMode = m_configurations->getModeValue(CONFIGURATION_SHOT_MODE);

    if (getCameraId() != CAMERA_ID_FRONT
        || m_configurations->getZoomRatio() != 1.0f
        || m_configurations->getMode(CONFIGURATION_RECORDING_MODE) == true
        || shotMode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_COLOR_IRIS
        || shotMode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_FACE_LOCK
        || shotMode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_INTELLIGENT_SCAN
        || shotMode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_STICKER) {
        return false;
    }

    if (m_configurations->getSamsungCamera() == true) {
        if (m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_NONE
            && (shotMode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_AUTO
                || shotMode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_BEAUTY
                || shotMode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELFIE_FOCUS)) {
            return true;
        } else {
            return false;
        }
    } else { // default on for 3rd party app
        return true;
    }

    return false;
}
#endif

#ifdef SAMSUNG_HIFI_VIDEO
bool ExynosCameraParameters::isValidHiFiVideoSize(int w, int h, int fps)
{
    bool isValid = false;
    int sizeListLen = 0;
    int solutionSize[][HIFIVIDEO_SOLUTION_SIZE_MAX] = {
        /* videoW, videoH, fps */
        {2560, 1440, 30}, /* QHD */
        {1920, 1080, 30}, /* FHD */
        {2224, 1080, 30}, /* 18.5:9 */
        {1440, 1440, 30}, /* 1:1 */
        {1280,  720, 30}, /* HD */
    };

#ifdef SAMSUNG_SW_VDIS
    int solutionSizeWithVdis[][HIFIVIDEO_SOLUTION_SIZE_MAX] = {
        /* videoW, videoH, fps */
        {1920, 1080, 30}, /* FHD */
        {2224, 1080, 30}, /* 18.5:9 */
        {1280,  720, 30}, /* HD */
    };
#endif

#ifdef SAMSUNG_SW_VDIS
    bool isSwVdisMode = false;
    if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
        isSwVdisMode = true;
        sizeListLen = (sizeof(solutionSizeWithVdis) / HIFIVIDEO_SOLUTION_SIZE_MAX) / sizeof(int);
    } else
#endif
    {
        sizeListLen = (sizeof(solutionSize) / HIFIVIDEO_SOLUTION_SIZE_MAX) / sizeof(int);
    }

    for (int i = 0; i < sizeListLen; i++) {
#ifdef SAMSUNG_SW_VDIS
        if (isSwVdisMode == true) {
            if (solutionSizeWithVdis[i][HIFIVIDEO_SOLUTION_SIZE_VIDEO_W] == w
                && solutionSizeWithVdis[i][HIFIVIDEO_SOLUTION_SIZE_VIDEO_H] == h
                && solutionSizeWithVdis[i][HIFIVIDEO_SOLUTION_SIZE_VIDEO_FPS] == fps) {
                isValid = true;
                break;
            }
        } else
#endif
        {
            if (solutionSize[i][HIFIVIDEO_SOLUTION_SIZE_VIDEO_W] == w
                && solutionSize[i][HIFIVIDEO_SOLUTION_SIZE_VIDEO_H] == h
                && solutionSize[i][HIFIVIDEO_SOLUTION_SIZE_VIDEO_FPS] == fps) {
                isValid = true;
                break;
            }
        }
    }
    return isValid;
}

void ExynosCameraParameters::getHiFiVideoYuvSize(int w, int h, int fps, int *newW, int *newH)
{
    int *videoSizeList = NULL;
    int videoSizeListMax = 0;

    if (w < 0 || h < 0) {
        return;
    }

    videoSizeListMax = m_staticInfo->availableVideoListMax;
    for (int i = 0; i < videoSizeListMax; i++) {
        videoSizeList = m_staticInfo->availableVideoList[i];
        if (w == videoSizeList[0] && h == videoSizeList[1] && fps == (videoSizeList[3] / 1000)) {
            if (m_configurations->getModeValue(CONFIGURATION_HIFIVIDEO_OPMODE) == HIFIVIDEO_OPMODE_HIFIVDIS_RECORDING) {
                *newW = videoSizeList[4];
                *newH = videoSizeList[5];
            } else {
#ifdef HIFIVIDEO_INPUTCROP_MARGIN
                *newW = ALIGN_UP((int)(videoSizeList[0] * HIFIVIDEO_INPUTCROP_MARGIN), 2);
                *newH = ALIGN_UP((int)(videoSizeList[1] * HIFIVIDEO_INPUTCROP_MARGIN), 2);
#else
                *newW = videoSizeList[0];
                *newH = videoSizeList[1];
#endif
            }
            break;
        }
    }
}

void ExynosCameraParameters::getHiFiVideoAdjustYuvSize(int *width, int *height, int fps)
{
    int *videoSizeList = NULL;
    int videoSizeListMax = m_staticInfo->availableVideoListMax;
    if (m_configurations->getModeValue(CONFIGURATION_HIFIVIDEO_OPMODE) == HIFIVIDEO_OPMODE_HIFIVDIS_RECORDING) {
        for (int i = 0; i < videoSizeListMax; i++) {
            videoSizeList = m_staticInfo->availableVideoList[i];
            if (*width == videoSizeList[4] && *height == videoSizeList[5] && fps == (videoSizeList[3] / 1000)) {
                *width = videoSizeList[0];
                *height = videoSizeList[1];
                break;
            }
        }
    }
#ifdef HIFIVIDEO_INPUTCROP_MARGIN
    else {
        for (int i = 0; i < videoSizeListMax; i++) {
            videoSizeList = m_staticInfo->availableVideoList[i];
            int curW = ALIGN_UP((int)(videoSizeList[0] * HIFIVIDEO_INPUTCROP_MARGIN), 2);
            int curH = ALIGN_UP((int)(videoSizeList[1] * HIFIVIDEO_INPUTCROP_MARGIN), 2);
            if (*width == curW && *height == curH) {
                *width = videoSizeList[0];
                *height = videoSizeList[1];
                break;
            }
        }
    }
#endif
}

#ifdef HIFIVIDEO_ZOOM_SUPPORTED
void ExynosCameraParameters::getHiFiVideoCropRegion(ExynosRect *cropRegion)
{
    int hwSensorW, hwSensorH;
    float zoomRatio = m_configurations->getZoomRatio();

    if (cropRegion != NULL) {
        getSize(HW_INFO_HW_SENSOR_SIZE, (uint32_t *)&hwSensorW, (uint32_t *)&hwSensorH);
        cropRegion->w = ALIGN_UP((int)ceil((float)hwSensorW / zoomRatio), 2);
        cropRegion->h = ALIGN_UP((int)ceil((float)hwSensorH / zoomRatio), 2);
        cropRegion->x = ALIGN_DOWN((hwSensorW - cropRegion->w) / 2, 2);
        cropRegion->y = ALIGN_DOWN((hwSensorH - cropRegion->h) / 2, 2);
    }
}

void ExynosCameraParameters::getHiFiVideoScalerControlInfo(int vid, ExynosRect *refRect, ExynosRect *srcRect, ExynosRect *dstRect)
{
    ExynosRect ref;

    switch (vid) {
        case FIMC_IS_VIDEO_M0P_NUM:
            {
                /* No Upscale Output = Downscale or Bypass */
#ifdef USE_DUAL_CAMERA
                if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
                    if (srcRect->w < refRect->w || srcRect->h < refRect->h) {
                        CLOGV("[HIFIVIDEO] Upscale in MCSC!! (Src %dx%d Dst %dx%d)",
                                srcRect->w, srcRect->h, refRect->w, refRect->h);
                    }
                    return;
                }
#endif

#ifdef HIFIVIDEO_INPUTCROP_CENTEROUT
                if (dstRect->w != refRect->w) {
                    dstRect->x = ALIGN_DOWN(((dstRect->w - refRect->w) >> 1), 2);
                    dstRect->w = refRect->w;
                }
                if (dstRect->h != refRect->h) {
                    dstRect->y = ALIGN_DOWN(((dstRect->h - refRect->h) >> 1), 2);
                    dstRect->h = refRect->h;
                }
#endif
                if (srcRect->w < refRect->w) {
#ifdef HIFIVIDEO_INPUTCROP_CENTEROUT
                    dstRect->x = ALIGN_DOWN(((refRect->w - srcRect->w) >> 1), 2) + dstRect->x;
#endif
                    dstRect->w = srcRect->w;
                }
                if (srcRect->h < refRect->h) {
#ifdef HIFIVIDEO_INPUTCROP_CENTEROUT
                    dstRect->y = ALIGN_DOWN(((refRect->h - srcRect->h) >> 1), 2) + dstRect->y;
#endif
                    dstRect->h = srcRect->h;
                }
            }
            break;
        case FIMC_IS_VIDEO_M1P_NUM:
        case FIMC_IS_VIDEO_M2P_NUM:
        case FIMC_IS_VIDEO_M3P_NUM:
        case FIMC_IS_VIDEO_M4P_NUM:
        case FIMC_IS_VIDEO_M5P_NUM:
            {
                /* Upscale CropRegion for Picture & Thumbnail */
                getHiFiVideoCropRegion(&ref);
                if (srcRect->w > ref.w) {
                    dstRect->x = ALIGN_DOWN(((srcRect->w - ref.w) >> 1), 2);
                    dstRect->w = ref.w;
                } else {
                    dstRect->x = 0;
                    dstRect->w = srcRect->w;
                }
                if (srcRect->h > ref.h) {
                    dstRect->y = ALIGN_DOWN(((srcRect->h - ref.h) >> 1), 2);
                    dstRect->h = ref.h;
                } else {
                    dstRect->y = 0;
                    dstRect->h = srcRect->h;
                }
            }
            break;
        default:
            CLOGD("[HIFIVIDEO] Unknown Node Index!! (%d)", vid);
            break;
    }
}

void ExynosCameraParameters::getHiFiVideoZoomRatio(float viewZoomRatio, float *activeZoomRatio, int *activeMargin)
{
    if (viewZoomRatio > HIFIVIDEO_INPUTCROP_MAXZOOM) {
        *activeZoomRatio = HIFIVIDEO_INPUTCROP_MAXZOOM;
        *activeMargin = (int)((viewZoomRatio - HIFIVIDEO_INPUTCROP_MAXZOOM) / HIFIVIDEO_INPUTCROP_MAXZOOM * 100);
    } else {
        *activeZoomRatio = viewZoomRatio;
        *activeMargin = 0;
    }
}
#endif
#endif

#ifdef SAMSUNG_SW_VDIS
void ExynosCameraParameters::getSWVdisYuvSize(int w, int h, int fps, int *newW, int *newH)
{
    int *videoSizeList = NULL;
    int videoSizeListMax = m_staticInfo->availableVideoListMax;

    if (w < 0 || h < 0) {
        return;
    }

    for (int i = 0; i < videoSizeListMax; i++) {
        videoSizeList = m_staticInfo->availableVideoList[i];
        if (w == videoSizeList[VIDEO_WIDTH] && h == videoSizeList[VIDEO_HEIGHT] && fps == (videoSizeList[VIDEO_MAX_FPS] / 1000)) {
            *newW = videoSizeList[4];
            *newH = videoSizeList[5];
            break;
        }
    }
}

void ExynosCameraParameters::getSWVdisAdjustYuvSize(int *width, int *height, int fps)
{
    int *videoSizeList = NULL;
    int videoSizeListMax = m_staticInfo->availableVideoListMax;

    for (int i = 0; i < videoSizeListMax; i++) {
        videoSizeList = m_staticInfo->availableVideoList[i];
        if (*width == videoSizeList[VIDEO_VDIS_WIDTH] && *height == videoSizeList[VIDEO_VDIS_HEIGHT] && fps == (videoSizeList[VIDEO_MAX_FPS] / 1000)) {
            *width = videoSizeList[0];
            *height = videoSizeList[1];
            break;
        }
    }
}

bool ExynosCameraParameters::isSWVdisOnPreview(void)
{
    bool swVDIS_OnPreview = false;

    if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
        if (m_configurations->getDynamicMode(DYNAMIC_HIGHSPEED_RECORDING_MODE) == false
            && (getCameraId() == CAMERA_ID_BACK || getCameraId() == CAMERA_ID_BACK_1)) {
            swVDIS_OnPreview = true;
        }
    }

    return swVDIS_OnPreview;
}

#ifdef SAMSUNG_SW_VDIS_USE_OIS
void ExynosCameraParameters::setSWVdisMetaCtlOISCoef(uint32_t coef)
{
    setMetaCtlOISCoef(&m_metadata, coef);
}

void ExynosCameraParameters::setSWVdisPreviewFrameExposureTime(int exposureTime)
{
    if (exposureTime != 0) {
        m_SWVdisPreviewFrameExposureTime = exposureTime;
    }
}

int ExynosCameraParameters::getSWVdisPreviewFrameExposureTime(void)
{
    return m_SWVdisPreviewFrameExposureTime;
}
#endif/* SAMSUNG_SW_VDIS_USE_OIS */
#endif/* SAMSUNG_SW_VDIS */

#ifdef SAMSUNG_HIFI_VIDEO
void ExynosCameraParameters::setHFVZdebugInfo(unsigned char *data, unsigned int size)
{
    char sizeInfo[2] = {(unsigned char)((size >> 8) & 0xFF), (unsigned char)(size & 0xFF)};
    m_metaInfoLock.lock();
    memcpy(m_staticInfo->hifi_video_exif_info.hifi_video_exif, HIFI_VIDEO_EXIF_TAG, sizeof(HIFI_VIDEO_EXIF_TAG));
    memcpy(m_staticInfo->hifi_video_exif_info.hifi_video_exif + sizeof(HIFI_VIDEO_EXIF_TAG) - 1, sizeInfo, sizeof(sizeInfo));
    memcpy(m_staticInfo->hifi_video_exif_info.hifi_video_exif + sizeof(HIFI_VIDEO_EXIF_TAG) + sizeof(sizeInfo) - 1, data, size);
    m_metaInfoLock.unlock();
}
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
status_t ExynosCameraParameters::adjustDualSolutionSize(int targetWidth, int targetHeight)
{
    int sizeListLen = 0;
    int dualSolutionSize[][DUAL_SOLUTION_SIZE_MAX] = {
        /* dstW, dstH, srcWideW, srcWideH, srcTeleW, srcTeleH */
        /* 30% margin */
        {1920, 1080, 2496, 1408, 2496, 1408}, /* preview 16:9, FHD */
        {1440, 1080, 1872, 1408, 1872, 1408}, /* preview 4:3 */
        {1440, 1440, 1872, 1872, 1872, 1872}, /* preview 1:1 */
        {1088, 1088, 1424, 1424, 1424, 1424}, /* preview 1:1 */
        {2224, 1080, 2896, 1408, 2896, 1408}, /* preview 18.5:9 */
        {3840, 2160, 3840, 2160, 3840, 2160},  /* UHD */
        {2560, 1440, 3328, 1872, 3328, 1872},  /* QHD */
        {1280, 720,  1664, 944,  1664, 944},  /* HD */
        {960,  720,  1248, 944,  1248, 944},  /* preview 4:3 */
        {640,  480,  832,  624,  832,  624},  /* VGA */
    };

int dualSolutionSize_Margin20[][DUAL_SOLUTION_SIZE_MAX] = {
        /* dstW, dstH, srcWideW, srcWideH, srcTeleW, srcTeleH */
        /* 20% margin */
        {1920, 1080, 2304, 1296, 2304, 1296}, /* preview 16:9, FHD */
        {1440, 1080, 1728, 1296, 1728, 1296}, /* preview 4:3 */
        {1440, 1440, 1728, 1728, 1728, 1728}, /* preview 1:1 */
        {1088, 1088, 1312, 1312, 1312, 1312}, /* preview 1:1 */
        {2224, 1080, 2672, 1296, 2672, 1296}, /* preview 18.5:9 */
        {3840, 2160, 3840, 2160, 3840, 2160},  /* UHD */
        {2560, 1440, 3072, 1728, 3072, 1728},  /* QHD */
        {1280, 720,  1536, 864,  1536, 864},  /* HD */
        {960,  720,  1152, 864,  1152, 864},  /* preview 4:3 */
        {640,  480,  768,  576,  768,  576},  /* VGA */
    };

#ifdef SAMSUNG_SW_VDIS
    int dualSolutionVdisSize[][DUAL_SOLUTION_SIZE_MAX] = {
        /* dstW, dstH, srcWideW, srcWideH, srcTeleW, srcTeleH */
        /* 30% margin */
#ifdef SAMSUNG_SW_VDIS_UHD_20MARGIN
        {4608, 2592, 4608, 2592, 4608, 2592},  /* UHD */
#else
        {4032, 2268, 4032, 2268, 4032, 2268},  /* UHD */
#endif
        {2304, 1296, 3008, 1696, 3008, 1696},  /* FHD */
        {2672, 1296, 3488, 1696, 3488, 1696},  /* 18.5:9 */
        {3072, 1728, 4000, 2256, 4000, 2256},  /* QHD */
        {1536, 864,  2000, 1136, 2000, 1136},  /* HD */
    };

    int dualSolutionVdisSize_Margin20[][DUAL_SOLUTION_SIZE_MAX] = {
        /* dstW, dstH, srcWideW, srcWideH, srcTeleW, srcTeleH */
        /* 20% margin */
#ifdef SAMSUNG_SW_VDIS_UHD_20MARGIN
        {4608, 2592, 4608, 2592, 4608, 2592},  /* UHD */
#else
        {4032, 2268, 4032, 2268, 4032, 2268},  /* UHD */
#endif
        {2304, 1296, 2784, 1566, 2784, 1566},  /* FHD */
        {2672, 1296, 3216, 1558, 3216, 1558},  /* 18.5:9 */
        {3072, 1728, 3696, 2078, 3696, 2078},  /* QHD */
        {1536, 864,  1856, 1044, 1856, 1044},  /* HD */
    };

#endif

#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_ZOOM_SUPPORTED)
    int dualSolutionHiFiVideoSize[][DUAL_SOLUTION_SIZE_MAX] = {
        /* dstW, dstH, srcWideW, srcWideH, srcTeleW, srcTeleH */
        /* 20% margin */
        {3840, 2160, 3840, 2160, 3840, 2160},  /* UHD */
        {2560, 1440, 3072, 1728, 3072, 1728},  /* QHD */
        {1920, 1080, 2304, 1296, 2304, 1296},  /* FHD */
        {2224, 1080, 2672, 1296, 2672, 1296},  /* 18.5:9 */
        {1440, 1440, 1872, 1872, 1872, 1872},  /* 1:1 */
        {1280, 720,  1536, 864,  1536, 864},   /* HD */
    };

    int dualSolutionHiFiVideoVdisSize[][DUAL_SOLUTION_SIZE_MAX] = {
        /* dstW, dstH, srcWideW, srcWideH, srcTeleW, srcTeleH */
        /* 20% margin */
        {4608, 2592, 4608, 2592, 4608, 2592},  /* UHD */
        {3072, 1728, 3696, 2078, 3696, 2078},  /* QHD */
        {2304, 1296, 2784, 1566, 2784, 1566},  /* FHD */
        {2672, 1296, 3216, 1558, 3216, 1558},  /* 18.5:9 */
        {1440, 1440, 1872, 1872, 1872, 1872},  /* 1:1 */
        {1536, 864,  1856, 1044, 1856, 1044},  /* HD */
    };
#endif

#ifdef SAMSUNG_SW_VDIS
    bool isSwVdisMode = false;
    int vdisW = 0, vdisH = 0;
    int videoW = 0, videoH = 0;

    if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
        isSwVdisMode = true;

        sizeListLen = (sizeof(dualSolutionVdisSize) / DUAL_SOLUTION_SIZE_MAX) / sizeof(int);
    } else
#endif
#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_ZOOM_SUPPORTED)
    if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
        if (m_configurations->getModeValue(CONFIGURATION_HIFIVIDEO_OPMODE) == HIFIVIDEO_OPMODE_HIFIVDIS_RECORDING ||
            m_configurations->getModeValue(CONFIGURATION_HIFIVIDEO_OPMODE) == HIFIVIDEO_OPMODE_VDISONLY_RECORDING) {
            sizeListLen = (sizeof(dualSolutionHiFiVideoVdisSize) / DUAL_SOLUTION_SIZE_MAX) / sizeof(int);
        } else {
            sizeListLen = (sizeof(dualSolutionHiFiVideoSize) / DUAL_SOLUTION_SIZE_MAX) / sizeof(int);
        }
    } else
#endif
    {
        sizeListLen = (sizeof(dualSolutionSize) / DUAL_SOLUTION_SIZE_MAX) / sizeof(int);
    }

    int i;
    for (i = 0; i < sizeListLen; i++) {
#ifdef SAMSUNG_SW_VDIS
        if (isSwVdisMode == true) {
            if (dualSolutionVdisSize[i][DUAL_SOLUTION_SIZE_DST_W] == targetWidth &&
                    dualSolutionVdisSize[i][DUAL_SOLUTION_SIZE_DST_H] == targetHeight) {
                m_dualDstWidth       = dualSolutionVdisSize[i][DUAL_SOLUTION_SIZE_DST_W];
                m_dualDstHeight      = dualSolutionVdisSize[i][DUAL_SOLUTION_SIZE_DST_H];
                m_dualSrcWideWidth   = dualSolutionVdisSize[i][DUAL_SOLUTION_SIZE_SRC_WIDE_W];
                m_dualSrcWideHeight  = dualSolutionVdisSize[i][DUAL_SOLUTION_SIZE_SRC_WIDE_H];
                m_dualSrcTeleWidth   = dualSolutionVdisSize[i][DUAL_SOLUTION_SIZE_SRC_TELE_W];
                m_dualSrcTeleHeight  = dualSolutionVdisSize[i][DUAL_SOLUTION_SIZE_SRC_TELE_H];

                m_dualMargin20SrcWideWidth   = dualSolutionVdisSize_Margin20[i][DUAL_SOLUTION_SIZE_SRC_WIDE_W];
                m_dualMargin20SrcWideHeight  = dualSolutionVdisSize_Margin20[i][DUAL_SOLUTION_SIZE_SRC_WIDE_H];
                m_dualMargin20SrcTeleWidth   = dualSolutionVdisSize_Margin20[i][DUAL_SOLUTION_SIZE_SRC_TELE_W];
                m_dualMargin20SrcTeleHeight  = dualSolutionVdisSize_Margin20[i][DUAL_SOLUTION_SIZE_SRC_TELE_H];
                break;
            }
        } else
#endif
#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_ZOOM_SUPPORTED)
        if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
            if (m_configurations->getModeValue(CONFIGURATION_HIFIVIDEO_OPMODE) == HIFIVIDEO_OPMODE_HIFIVDIS_RECORDING ||
                m_configurations->getModeValue(CONFIGURATION_HIFIVIDEO_OPMODE) == HIFIVIDEO_OPMODE_VDISONLY_RECORDING) {
                if (dualSolutionHiFiVideoVdisSize[i][DUAL_SOLUTION_SIZE_DST_W] == targetWidth &&
                    dualSolutionHiFiVideoVdisSize[i][DUAL_SOLUTION_SIZE_DST_H] == targetHeight) {
                    m_dualDstWidth       = dualSolutionHiFiVideoVdisSize[i][DUAL_SOLUTION_SIZE_DST_W];
                    m_dualDstHeight      = dualSolutionHiFiVideoVdisSize[i][DUAL_SOLUTION_SIZE_DST_H];
                    m_dualSrcWideWidth   = dualSolutionHiFiVideoVdisSize[i][DUAL_SOLUTION_SIZE_SRC_WIDE_W];
                    m_dualSrcWideHeight  = dualSolutionHiFiVideoVdisSize[i][DUAL_SOLUTION_SIZE_SRC_WIDE_H];
                    m_dualSrcTeleWidth   = dualSolutionHiFiVideoVdisSize[i][DUAL_SOLUTION_SIZE_SRC_TELE_W];
                    m_dualSrcTeleHeight  = dualSolutionHiFiVideoVdisSize[i][DUAL_SOLUTION_SIZE_SRC_TELE_H];

                    m_dualMargin20SrcWideWidth   = dualSolutionHiFiVideoVdisSize[i][DUAL_SOLUTION_SIZE_SRC_WIDE_W];
                    m_dualMargin20SrcWideHeight  = dualSolutionHiFiVideoVdisSize[i][DUAL_SOLUTION_SIZE_SRC_WIDE_H];
                    m_dualMargin20SrcTeleWidth   = dualSolutionHiFiVideoVdisSize[i][DUAL_SOLUTION_SIZE_SRC_TELE_W];
                    m_dualMargin20SrcTeleHeight  = dualSolutionHiFiVideoVdisSize[i][DUAL_SOLUTION_SIZE_SRC_TELE_H];
                    break;
                }
            } else {
                if (dualSolutionHiFiVideoSize[i][DUAL_SOLUTION_SIZE_DST_W] == targetWidth &&
                    dualSolutionHiFiVideoSize[i][DUAL_SOLUTION_SIZE_DST_H] == targetHeight) {
                    m_dualDstWidth       = dualSolutionHiFiVideoSize[i][DUAL_SOLUTION_SIZE_DST_W];
                    m_dualDstHeight      = dualSolutionHiFiVideoSize[i][DUAL_SOLUTION_SIZE_DST_H];
                    m_dualSrcWideWidth   = dualSolutionHiFiVideoSize[i][DUAL_SOLUTION_SIZE_SRC_WIDE_W];
                    m_dualSrcWideHeight  = dualSolutionHiFiVideoSize[i][DUAL_SOLUTION_SIZE_SRC_WIDE_H];
                    m_dualSrcTeleWidth   = dualSolutionHiFiVideoSize[i][DUAL_SOLUTION_SIZE_SRC_TELE_W];
                    m_dualSrcTeleHeight  = dualSolutionHiFiVideoSize[i][DUAL_SOLUTION_SIZE_SRC_TELE_H];

                    m_dualMargin20SrcWideWidth   = dualSolutionHiFiVideoSize[i][DUAL_SOLUTION_SIZE_SRC_WIDE_W];
                    m_dualMargin20SrcWideHeight  = dualSolutionHiFiVideoSize[i][DUAL_SOLUTION_SIZE_SRC_WIDE_H];
                    m_dualMargin20SrcTeleWidth   = dualSolutionHiFiVideoSize[i][DUAL_SOLUTION_SIZE_SRC_TELE_W];
                    m_dualMargin20SrcTeleHeight  = dualSolutionHiFiVideoSize[i][DUAL_SOLUTION_SIZE_SRC_TELE_H];
                    break;
                }
            }
        } else
#endif
        {
            if (dualSolutionSize[i][DUAL_SOLUTION_SIZE_DST_W] == targetWidth &&
                    dualSolutionSize[i][DUAL_SOLUTION_SIZE_DST_H] == targetHeight) {
                m_dualDstWidth       = dualSolutionSize[i][DUAL_SOLUTION_SIZE_DST_W];
                m_dualDstHeight      = dualSolutionSize[i][DUAL_SOLUTION_SIZE_DST_H];
                m_dualSrcWideWidth   = dualSolutionSize[i][DUAL_SOLUTION_SIZE_SRC_WIDE_W];
                m_dualSrcWideHeight  = dualSolutionSize[i][DUAL_SOLUTION_SIZE_SRC_WIDE_H];
                m_dualSrcTeleWidth   = dualSolutionSize[i][DUAL_SOLUTION_SIZE_SRC_TELE_W];
                m_dualSrcTeleHeight  = dualSolutionSize[i][DUAL_SOLUTION_SIZE_SRC_TELE_H];

                m_dualMargin20SrcWideWidth   = dualSolutionSize_Margin20[i][DUAL_SOLUTION_SIZE_SRC_WIDE_W];
                m_dualMargin20SrcWideHeight  = dualSolutionSize_Margin20[i][DUAL_SOLUTION_SIZE_SRC_WIDE_H];
                m_dualMargin20SrcTeleWidth   = dualSolutionSize_Margin20[i][DUAL_SOLUTION_SIZE_SRC_TELE_W];
                m_dualMargin20SrcTeleHeight  = dualSolutionSize_Margin20[i][DUAL_SOLUTION_SIZE_SRC_TELE_H];
                break;
            }
        }
    }

    if (i == sizeListLen)
        return BAD_VALUE;
    else
        return NO_ERROR;
}

void ExynosCameraParameters::getDualSolutionSize(int *dstW, int *dstH,
                                                      int *wideW, int *wideH, int *teleW, int *teleH, int margin)
{
    *dstW  = m_dualDstWidth;
    *dstH  = m_dualDstHeight;

    if (margin == DUAL_SOLUTION_MARGIN_VALUE_20) {
        *wideW = m_dualMargin20SrcWideWidth;
        *wideH = m_dualMargin20SrcWideHeight;
        *teleW = m_dualMargin20SrcTeleWidth;
        *teleH = m_dualMargin20SrcTeleHeight;
    } else
    {
        *wideW = m_dualSrcWideWidth;
        *wideH = m_dualSrcWideHeight;
        *teleW = m_dualSrcTeleWidth;
        *teleH = m_dualSrcTeleHeight;
    }

    return;
}

int ExynosCameraParameters::getDualSolutionMarginValue(ExynosRect* targetRect, ExynosRect* SrcRect)
{
    ExynosRect rect;
    int *srcWidth, *srcHeight;

    if (m_cameraId == CAMERA_ID_BACK_1) {
        srcWidth = &m_dualMargin20SrcTeleWidth;
        srcHeight = &m_dualMargin20SrcTeleHeight;
    } else {
        srcWidth = &m_dualMargin20SrcWideWidth;
        srcHeight = &m_dualMargin20SrcWideHeight;
    }

    if (SrcRect->w == *srcWidth && SrcRect->h == *srcHeight)
        return DUAL_SOLUTION_MARGIN_VALUE_20;

    if (m_cameraId == CAMERA_ID_BACK_1) {
        srcWidth = &m_dualSrcTeleWidth;
        srcHeight = &m_dualSrcTeleHeight;
    } else {
        srcWidth = &m_dualSrcWideWidth;
        srcHeight = &m_dualSrcWideHeight;
    }

    if (SrcRect->w == *srcWidth && SrcRect->h == *srcHeight)
        return DUAL_SOLUTION_MARGIN_VALUE_30;

    return 0;
}

status_t ExynosCameraParameters::getFusionSize(int w, int h, ExynosRect *srcRect, ExynosRect *dstRect, int margin)
{
    status_t ret = NO_ERROR;

    ret = m_getFusionSize(w, h, srcRect, true, margin);
    if (ret != NO_ERROR) {
        CLOGE("m_getFusionSize(%d, %d, true) fail", w, h);
        return ret;
    }

    ret = m_getFusionSize(w, h, dstRect, false, margin);
    if (ret != NO_ERROR) {
        CLOGE("m_getFusionSize(%d, %d, false) fail", w, h);
        return ret;
    }

    return ret;
}

status_t ExynosCameraParameters::m_getFusionSize(int w, int h, ExynosRect *rect, bool flagSrc, int margin)
{
    status_t ret = NO_ERROR;

    if (w <= 0 || h <= 0) {
        CLOGE("w(%d) <= 0 || h(%d) <= 0", w, h);
        return INVALID_OPERATION;
    }

    rect->x = 0;
    rect->y = 0;

    rect->w = w;
    rect->h = h;

    rect->fullW = rect->w;
    rect->fullH = rect->h;

    rect->colorFormat = getHwPreviewFormat();

    int wideW, wideH, teleW, teleH, dstW, dstH;
    getDualSolutionSize(&dstW, &dstH, &wideW, &wideH, &teleW, &teleH, margin);
    if (flagSrc == true) {
        if (m_cameraId == CAMERA_ID_BACK) {
            rect->w = wideW;
            rect->h = wideH;
        } else if (m_cameraId == CAMERA_ID_BACK_1) {
            rect->w = teleW;
            rect->h = teleH;
        }
    } else {
        rect->w = dstW;
        rect->h = dstH;
    }

    rect->fullW = rect->w;
    rect->fullH = rect->h;

    return ret;
}
#endif

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
void ExynosCameraParameters::setFusionCapturedebugInfo(unsigned char *data, unsigned int size)
{
    char sizeInfo[2] = {(unsigned char)((size >> 8) & 0xFF), (unsigned char)(size & 0xFF)};

    memset(m_staticInfo->lls_exif_info.lls_exif, 0, LLS_EXIF_SIZE);
    memcpy(m_staticInfo->lls_exif_info.lls_exif, FUSION_EXIF_TAG, sizeof(FUSION_EXIF_TAG));
    memcpy(m_staticInfo->lls_exif_info.lls_exif + sizeof(FUSION_EXIF_TAG) - 1, sizeInfo, sizeof(sizeInfo));
    memcpy(m_staticInfo->lls_exif_info.lls_exif + sizeof(FUSION_EXIF_TAG) + sizeof(sizeInfo)- 1, data, size);
}

bool ExynosCameraParameters::useCaptureExtCropROI(void)
{
    bool ret = false;

    if (m_configurations->getZoomRatio() > 1.0f
        && m_scenario == SCENARIO_DUAL_REAR_ZOOM
        && m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false
        && !(m_configurations->getDualOperationModeReprocessing() == DUAL_OPERATION_MODE_SYNC
              && getCameraId() == CAMERA_ID_BACK_1)
    ) {
        ret = true;
    }

    return ret;
}

void ExynosCameraParameters::setCaptureExtCropROI(UTrect captureExtCropROI)
{
    status_t ret = NO_ERROR;
    ExynosRect activeArraySize;
    ExynosRect hwSensorSize;
    ExynosRect hwActiveArraySize;
    ExynosRect hwBcropSize;
    ExynosRect inputCropROI;
    int offsetX = 0, offsetY = 0;
    int pictureW = 0, pictureH = 0;
    float scaleRatioW = 0.0f, scaleRatioH = 0.0f;

    m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)&pictureW, (uint32_t *)&pictureH);
    getSize(HW_INFO_MAX_SENSOR_SIZE, (uint32_t *)&activeArraySize.w, (uint32_t *)&activeArraySize.h);
    getPreviewBayerCropSize(&hwSensorSize, &hwBcropSize);

    inputCropROI.x = captureExtCropROI.left;
    inputCropROI.y = captureExtCropROI.top;
    inputCropROI.w = captureExtCropROI.right - captureExtCropROI.left + 1;
    inputCropROI.h = captureExtCropROI.bottom - captureExtCropROI.top + 1;

    CLOGV2("ActiveArraySize %dx%d(%d) Region %d,%d %d,%d %dx%d(%d) HWSensorSize %dx%d(%d) HWBcropSize %d,%d %dx%d(%d)",
            activeArraySize.w, activeArraySize.h, SIZE_RATIO(activeArraySize.w, activeArraySize.h),
            captureExtCropROI.left, captureExtCropROI.top, captureExtCropROI.right, captureExtCropROI.bottom,
            captureExtCropROI.right - captureExtCropROI.left, captureExtCropROI.bottom - captureExtCropROI.top,
            SIZE_RATIO(captureExtCropROI.right - captureExtCropROI.left, captureExtCropROI.bottom - captureExtCropROI.top),
            hwSensorSize.w, hwSensorSize.h, SIZE_RATIO(hwSensorSize.w, hwSensorSize.h),
            hwBcropSize.x, hwBcropSize.y, hwBcropSize.w, hwBcropSize.h, SIZE_RATIO(hwBcropSize.w, hwBcropSize.h));

    /* Calculate HW active array size for current sensor aspect ratio
       based on active array size
     */
    ret = getCropRectAlign(activeArraySize.w, activeArraySize.h,
                           hwSensorSize.w, hwSensorSize.h,
                           &hwActiveArraySize.x, &hwActiveArraySize.y,
                           &hwActiveArraySize.w, &hwActiveArraySize.h,
                           2, 2, 1.0f);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getCropRectAlign. Src %dx%d, Dst %dx%d",
                activeArraySize.w, activeArraySize.h,
                hwSensorSize.w, hwSensorSize.h);
        return;
    }

    CLOGV2("inputCropROI(%d,%d,%d,%d)(%d)",
        inputCropROI.x, inputCropROI.y, inputCropROI.w, inputCropROI.h, SIZE_RATIO(inputCropROI.w, inputCropROI.h));
    offsetX = inputCropROI.x;
    offsetY = inputCropROI.y;

    /* Convert aspect ratio based on picture ratio */
    ret = getCropRectAlign(inputCropROI.w, inputCropROI.h,
                           pictureW, pictureH,
                           &inputCropROI.x, &inputCropROI.y,
                           &inputCropROI.w, &inputCropROI.h,
                           2, 2, 1.0f);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getCropRectAlign. Src %dx%d, Dst %dx%d",
                activeArraySize.w, activeArraySize.h,
                hwSensorSize.w, hwSensorSize.h);
        return;
    }
    inputCropROI.x += offsetX;
    inputCropROI.y += offsetY;
    CLOGV2("after inputCropROI(%d,%d,%d,%d)(%d)",
        inputCropROI.x, inputCropROI.y, inputCropROI.w, inputCropROI.h, SIZE_RATIO(inputCropROI.w, inputCropROI.h));

    /* Scale down the target region & HW active array size
       to adjust them to the input size without sensor margin
    */
    scaleRatioW = (float) hwSensorSize.w / (float) hwActiveArraySize.w;
    scaleRatioH = (float) hwSensorSize.h / (float) hwActiveArraySize.h;

    inputCropROI.x = (int) (((float) inputCropROI.x) * scaleRatioW);
    inputCropROI.y = (int) (((float) inputCropROI.y) * scaleRatioH);
    inputCropROI.w = (int) (((float) inputCropROI.w) * scaleRatioW);
    inputCropROI.h = (int) (((float) inputCropROI.h) * scaleRatioH);

    hwActiveArraySize.x = (int) (((float) hwActiveArraySize.x) * scaleRatioW);
    hwActiveArraySize.y = (int) (((float) hwActiveArraySize.y) * scaleRatioH);
    hwActiveArraySize.w = (int) (((float) hwActiveArraySize.w) * scaleRatioW);
    hwActiveArraySize.h = (int) (((float) hwActiveArraySize.h) * scaleRatioH);

    /* Remove HW active array size offset */
    /* x-y */
    if (inputCropROI.x < hwActiveArraySize.x) inputCropROI.x = 0;
    else inputCropROI.x -= hwActiveArraySize.x;
    if (inputCropROI.y < hwActiveArraySize.y) inputCropROI.y = 0;
    else inputCropROI.y -= hwActiveArraySize.y;

    /* w-h */
    if (inputCropROI.w > hwActiveArraySize.w) {
        inputCropROI.x = 0;
        inputCropROI.w = hwActiveArraySize.w;
    } else if (inputCropROI.w + inputCropROI.x > hwActiveArraySize.w) {
        inputCropROI.x = ALIGN_DOWN((hwActiveArraySize.w - inputCropROI.w) >> 1, 2);
    }
    if (inputCropROI.h > hwActiveArraySize.h) {
        inputCropROI.y = 0;
        inputCropROI.h = hwActiveArraySize.h;
    } else if (inputCropROI.h + inputCropROI.y > hwActiveArraySize.h) {
        inputCropROI.y = ALIGN_DOWN((hwActiveArraySize.h - inputCropROI.h) >> 1, 2);
    }

    /* Adjust the boundary of HW bcrop size */
    /* x-y */
    if (inputCropROI.x < hwBcropSize.x) inputCropROI.x = 0;
    else inputCropROI.x -= hwBcropSize.x;
    if (inputCropROI.y < hwBcropSize.y) inputCropROI.y = 0;
    else inputCropROI.y -= hwBcropSize.y;

    /* w-h */
    if (inputCropROI.w > hwBcropSize.w) {
        inputCropROI.x = 0;
        inputCropROI.w = hwBcropSize.w;
    } else if (inputCropROI.w + inputCropROI.x > hwBcropSize.w) {
        inputCropROI.x = ALIGN_DOWN((hwBcropSize.w - inputCropROI.w) >> 1, 2);
    }
    if (inputCropROI.h > hwBcropSize.h) {
        inputCropROI.y = 0;
        inputCropROI.h = hwBcropSize.h;
    } else if (inputCropROI.h + inputCropROI.y > hwBcropSize.h) {
        inputCropROI.y = ALIGN_DOWN((hwBcropSize.h - inputCropROI.h) >> 1, 2);
    }

    CLOGV2("Region %d,%d %d,%d %dx%d(%d)",
            inputCropROI.x, inputCropROI.y, inputCropROI.w, inputCropROI.h,
            inputCropROI.w, inputCropROI.h,
            SIZE_RATIO(inputCropROI.w, inputCropROI.h));

    m_captureExtCropROI.x = ALIGN_DOWN(inputCropROI.x, 2);
    m_captureExtCropROI.y = ALIGN_DOWN(inputCropROI.y, 2);
    m_captureExtCropROI.w = inputCropROI.w;
    m_captureExtCropROI.h = inputCropROI.h;

    CLOGD2("captureCropROI(%d,%d,%d,%d)",
            m_captureExtCropROI.x,
            m_captureExtCropROI.y,
            m_captureExtCropROI.w,
            m_captureExtCropROI.h);
}
#endif

status_t ExynosCameraParameters::m_vendorConstructorInitalize(__unused int cameraId)
{
    status_t ret = NO_ERROR;
#ifdef USE_LLS_REPROCESSING
    m_LLSCaptureOn = false;
    m_LLSCaptureCount = 1;
#endif

#ifdef TEST_LLS_REPROCESING
    /* Always enable LLS Capture */
    m_LLSOn = true;
    m_LLSCaptureOn = true;
#endif

#ifdef SAMSUNG_OIS
    if (cameraId == CAMERA_ID_BACK) {
        mDebugInfo.debugSize[APP_MARKER_4] += sizeof(struct ois_exif_data);
    }
#endif
#ifdef SAMSUNG_MTF
    mDebugInfo.debugSize[APP_MARKER_4] += sizeof(struct mtf_exif_data);
#endif
    // Check debug_attribute_t struct in ExynosExif.h
#ifdef SAMSUNG_LLS_DEBLUR
    mDebugInfo.debugSize[APP_MARKER_4] += sizeof(struct lls_exif_data);
#endif

#ifdef SAMSUNG_LENS_DC
    mDebugInfo.debugSize[APP_MARKER_4] += sizeof(struct ldc_exif_data);
#endif
#ifdef SAMSUNG_STR_CAPTURE
    mDebugInfo.debugSize[APP_MARKER_4] += sizeof(struct str_exif_data);
#endif

#ifdef SAMSUNG_BD
    mDebugInfo.idx[1][0] = APP_MARKER_5;
    mDebugInfo.debugSize[APP_MARKER_5] += sizeof(struct bd_exif_data);
#endif

#ifdef SAMSUNG_UTC_TS
    mDebugInfo.idx[1][0] = APP_MARKER_5; /* mathcing the app marker 5 */
    mDebugInfo.debugSize[APP_MARKER_5] += sizeof(struct utc_ts);
#endif

#ifdef SAMSUNG_GYRO
    /* Initialize gyro value for inital-calibration */
    m_metadata.shot.uctl.aaUd.gyroInfo.x = 1234;
    m_metadata.shot.uctl.aaUd.gyroInfo.y = 1234;
    m_metadata.shot.uctl.aaUd.gyroInfo.z = 1234;
#endif

#ifdef SAMSUNG_ACCELEROMETER
    /* Initialize accelerometer value for inital-calibration */
    m_metadata.shot.uctl.aaUd.accInfo.x = 1234;
    m_metadata.shot.uctl.aaUd.accInfo.y = 1234;
    m_metadata.shot.uctl.aaUd.accInfo.z = 1234;
#endif

#ifdef SAMSUNG_TN_FEATURE
#ifdef USE_BINNING_MODE
    m_binningProperty = checkBinningProperty();
#endif
#endif

#ifdef LLS_CAPTURE
    m_LLSValue = 0;
    memset(m_needLLS_history, 0x00, sizeof(int) * LLS_HISTORY_COUNT);
#endif

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    m_captureExtCropROI = {0, };
#endif /* SAMSUNG_DUAL_ZOOM_CAPTURE*/

#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
    m_correctionTime = 0;

    if (m_cameraId == CAMERA_ID_BACK_1) {
#ifdef CORRECTION_SENSORFUSION_BACK_1
        m_correctionTime = CORRECTION_SENSORFUSION_BACK_1;
#endif
    } else {
#ifdef CORRECTION_SENSORFUSION_BACK_0
        m_correctionTime = CORRECTION_SENSORFUSION_BACK_0;
#endif
    }
#endif /* CORRECT_TIMESTAMP_FOR_SENSORFUSION */

    return ret;
}

void ExynosCameraParameters::m_vendorSWVdisMode(__unused bool *ret)
{
#ifdef SAMSUNG_SW_VDIS
    if (*ret == true
        && this->m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
        *ret = false;
    }
#endif
}

void ExynosCameraParameters::m_vendorSetExifChangedAttribute(__unused debug_attribute_t   &debugInfo,
                                                                          __unused unsigned int &offset,
                                                                          __unused bool &isWriteExif,
                                                                          __unused camera2_shot_t *shot,
                                                                          __unused bool useDebugInfo2)
{
#ifdef SAMSUNG_OIS
    if (getCameraId() == CAMERA_ID_BACK || getCameraId() == CAMERA_ID_BACK_1) {
        getOisEXIFFromFile(m_staticInfo, (int)shot->ctl.lens.opticalStabilizationMode);
        /* Copy ois data to debugData*/
        memcpy((void *)(debugInfo.debugData[APP_MARKER_4] + offset),
            (void *)&m_staticInfo->ois_exif_info, sizeof(m_staticInfo->ois_exif_info));
        offset += sizeof(m_staticInfo->ois_exif_info);
    }
#endif

#ifdef SAMSUNG_MTF
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    if (m_configurations->getMode(CONFIGURATION_FUSION_CAPTURE_MODE) == false)
#endif
    {
        getMTFdataEXIFFromFile(m_staticInfo, getCameraId(), shot->dm.lens.aperture);
        /* Copy mtf data to debugData*/
        memcpy((void *)(debugInfo.debugData[APP_MARKER_4] + offset),
            (void *)&m_staticInfo->mtf_exif_info, sizeof(m_staticInfo->mtf_exif_info));
        offset += sizeof(m_staticInfo->mtf_exif_info);
    }
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    int llsMode = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE);
    if (llsMode == MULTI_SHOT_MODE_MULTI1
        || llsMode == MULTI_SHOT_MODE_MULTI2
        || llsMode == MULTI_SHOT_MODE_MULTI3
        || llsMode == MULTI_SHOT_MODE_MULTI4
#ifdef SAMSUNG_MFHDR_CAPTURE
        || llsMode == MULTI_SHOT_MODE_MF_HDR
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
        || llsMode == MULTI_SHOT_MODE_LL_HDR
#endif
        || llsMode == MULTI_SHOT_MODE_SR) {
        memcpy((void *)(debugInfo.debugData[APP_MARKER_4] + offset),
            (void *)&m_staticInfo->lls_exif_info, sizeof(m_staticInfo->lls_exif_info));
        offset += sizeof(m_staticInfo->lls_exif_info);
        isWriteExif = true;
    }
#endif
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    if (m_configurations->getMode(CONFIGURATION_FUSION_CAPTURE_MODE)
        && isWriteExif == false) {
        CLOGD(" Fusion");
        memcpy((void *)(debugInfo.debugData[APP_MARKER_4] + offset),
            (void *)&m_staticInfo->lls_exif_info, sizeof(m_staticInfo->lls_exif_info));
        offset += sizeof(m_staticInfo->lls_exif_info);
        isWriteExif = true;
    }
#endif
#ifdef SAMSUNG_LENS_DC
    if (m_configurations->getMode(CONFIGURATION_LENS_DC_MODE)) {
        memcpy((void *)(debugInfo.debugData[APP_MARKER_4] + offset),
            (void *)&m_staticInfo->ldc_exif_info, sizeof(m_staticInfo->ldc_exif_info));
        offset += sizeof(m_staticInfo->ldc_exif_info);
    }
#endif

#ifdef SAMSUNG_STR_CAPTURE
    if (m_configurations->getMode(CONFIGURATION_STR_CAPTURE_MODE)) {
        memcpy((void *)(debugInfo.debugData[APP_MARKER_4] + offset),
           (void *)&m_staticInfo->str_exif_info, sizeof(m_staticInfo->str_exif_info));
        offset += sizeof(m_staticInfo->str_exif_info);
    }
#endif

#ifdef SAMSUNG_TN_FEATURE
    int realDataSize = 0;
    getSensorIdEXIFFromFile(m_staticInfo, getCameraId(), &realDataSize);
    /* Copy sensorId data to debugData*/
    memcpy((void *)(debugInfo.debugData[APP_MARKER_4] + offset),
        (void *)&m_staticInfo->sensor_id_exif_info, realDataSize);
    offset += realDataSize;
#endif

#ifdef SAMSUNG_HIFI_VIDEO
    if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE)) {
        m_metaInfoLock.lock();
        memcpy((void *)(debugInfo.debugData[APP_MARKER_4] + offset),
            (void *)&m_staticInfo->hifi_video_exif_info, sizeof(m_staticInfo->hifi_video_exif_info));
        offset += sizeof(m_staticInfo->hifi_video_exif_info);
        m_metaInfoLock.unlock();
    }
#endif

#ifdef SAMSUNG_UNI_API
    if (useDebugInfo2) {
        unsigned int appMarkerSize = (unsigned int)uni_appMarker_getSize(APP_MARKER_5);
        if (debugInfo.debugSize[APP_MARKER_5] > 0) {
            memset(debugInfo.debugData[APP_MARKER_5], 0, debugInfo.debugSize[APP_MARKER_5]);
        }

        if (appMarkerSize > 0 && debugInfo.debugSize[APP_MARKER_5] >= appMarkerSize) {
            char *flattenData = new char[appMarkerSize + 1];
            memset(flattenData, 0, appMarkerSize + 1);
            uni_appMarker_flatten(flattenData, APP_MARKER_5);
            memcpy(debugInfo.debugData[APP_MARKER_5], flattenData, appMarkerSize);
            delete[] flattenData;
        }
    }
#endif
}

#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
uint64_t ExynosCameraParameters::getCorrectTimeForSensorFusion(void)
{
    return m_correctionTime;
}
#endif

}; /* namespace android */
