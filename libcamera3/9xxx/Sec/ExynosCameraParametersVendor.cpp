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
                    {
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

    {
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
         {
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
        {
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
    {
        switch (shot->ctl.aa.aeMode) {
        case AA_AEMODE_CENTER:
                exifInfo->metering_mode = EXIF_METERING_CENTER;
                break;
        case AA_AEMODE_MATRIX:
                exifInfo->metering_mode = EXIF_METERING_AVERAGE;
                break;
        case AA_AEMODE_SPOT:
                exifInfo->metering_mode = EXIF_METERING_SPOT;
                break;
            default:
                exifInfo->metering_mode = EXIF_METERING_AVERAGE;
                break;
        }
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

    /* Image Unique ID */
#if defined(SENSOR_FW_GET_FROM_FILE)
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
    if (m_configurations->getMode(CONFIGURATION_VISION_MODE) == true) {
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

    if (zoomRatio == activeZoomRatio) {
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
    m_depthMapW = 0;
    m_depthMapH = 0;

#ifdef LLS_CAPTURE
    m_LLSValue = 0;
    memset(m_needLLS_history, 0x00, sizeof(int) * LLS_HISTORY_COUNT);
#endif

    return NO_ERROR;
}

void ExynosCameraParameters::getVendorRatioCropSizeForVRA(__unused ExynosRect *ratioCropSize)
{
}

void ExynosCameraParameters::getVendorRatioCropSize(__unused ExynosRect *ratioCropSize, __unused ExynosRect *mcscSize, __unused int portIndex, __unused int isReprocessing)
{
}

void ExynosCameraParameters::setImageUniqueId(char *uniqueId)
{
    memcpy(m_cameraInfo.imageUniqueId, uniqueId, sizeof(m_cameraInfo.imageUniqueId));
}

#ifdef LLS_CAPTURE
void ExynosCameraParameters::m_setLLSValue(struct camera2_shot_ext *shot)
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

    // Check debug_attribute_t struct in ExynosExif.h

#ifdef LLS_CAPTURE
    m_LLSValue = 0;
    memset(m_needLLS_history, 0x00, sizeof(int) * LLS_HISTORY_COUNT);
#endif

    return ret;
}

void ExynosCameraParameters::m_vendorSWVdisMode(__unused bool *ret)
{
}

void ExynosCameraParameters::m_vendorSetExifChangedAttribute(__unused debug_attribute_t   &debugInfo,
                                                                          __unused unsigned int &offset,
                                                                          __unused bool &isWriteExif,
                                                                          __unused camera2_shot_t *shot,
                                                                          __unused bool useDebugInfo2)
{
}

}; /* namespace android */
