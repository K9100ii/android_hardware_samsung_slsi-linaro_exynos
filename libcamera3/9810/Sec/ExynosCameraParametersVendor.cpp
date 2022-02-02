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
#include <cutils/log.h>

#include "ExynosCameraParameters.h"
#include "ExynosCameraMetadataConverter.h"

namespace android {

void ExynosCameraParameters::vendorSpecificConstructor(int cameraId)
{
    char propertyValue[PROPERTY_VALUE_MAX];
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

    m_binningProperty = 0;

#ifdef USE_LIMITATION_FOR_THIRD_PARTY
    m_fpsProperty = checkFpsProperty();
#endif

#ifdef SUPPORT_DEPTH_MAP
    m_flagUseDepthMap = false;
    m_depthMapW = 0;
    m_depthMapH = 0;
#endif

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

status_t ExynosCameraParameters::m_vendorInit(void)
{
    resetYuvStallPort();
    resetThumbnailCbSize();

#ifdef SUPPORT_DEPTH_MAP
    m_flagUseDepthMap = false;
    m_depthMapW = 0;
    m_depthMapH = 0;
#endif

    return OK;
}

status_t ExynosCameraParameters::checkVtMode(const CameraParameters& params)
{
    int newVTMode = params.getInt("vtmode");
    int curVTMode = -1;

    CLOGD("newVTMode (%d)", newVTMode);
    /*
     * VT mode
     *   1: 3G vtmode (176x144, Fixed 7fps)
     *   2: LTE or WIFI vtmode (640x480, Fixed 15fps)
     *   3: Reserved : Smart Stay
     *   4: CHINA vtmode (1280x720, Fixed 30fps)
     */
    if (newVTMode == 3 || (newVTMode < 0 || newVTMode > 4)) {
        newVTMode = 0;
    }

    curVTMode = getVtMode();

    if (curVTMode != newVTMode) {
        m_setVtMode(newVTMode);
    }

    return NO_ERROR;
}

int ExynosCameraParameters::getRecordingFps(void)
{
    return m_cameraInfo.recordingFps;
}

void ExynosCameraParameters::setRecordingFps(int fps)
{
    m_cameraInfo.recordingFps = fps;
}

status_t ExynosCameraParameters::checkRecordingFps(const CameraParameters& params)
{
    int newRecordingFps = params.getInt("recording-fps");

    CLOGD(" %d fps", newRecordingFps);
    setRecordingFps(newRecordingFps);

    return NO_ERROR;
}

status_t ExynosCameraParameters::checkDualMode(const CameraParameters& params)
{
    /* dual_mode */
    bool flagDualMode = false;
    const char *newDualMode = params.get("dual_mode");

    CLOGD(" newDualMode %s", newDualMode);

    if ((newDualMode != NULL) && ( !strcmp(newDualMode, "true"))) {
        flagDualMode = true;
    }

    setPIPMode(flagDualMode);

    return NO_ERROR;
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
        configMode = this->getConfigMode();
        switch (configMode) {
        case CONFIG_MODE::NORMAL:
            {
                if (getPIPMode() == true) {
                    previewSizelist = m_staticInfo->dualPreviewSizeLut;
                    previewSizeLutMax = m_staticInfo->previewSizeLutMax;
                } else {
                    previewSizelist = m_staticInfo->previewFullSizeLut;
                    previewSizeLutMax = m_staticInfo->previewFullSizeLutMax;
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
    int videoRatioEnum = SIZE_RATIO_16_9;
    int (*pictureSizelist)[SIZE_OF_LUT] = NULL;
    int pictureSizelistMax = 0;

    if (getPIPMode() == true) {
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
    getSensorMargin(&hwSensorMarginW, &hwSensorMarginH);
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

    getMaxSensorSize(&activeArraySize.w, &activeArraySize.h);

    if (isUseReprocessing3aaInputCrop() == true) {
        m_getCropRegion(&cropRegion.x, &cropRegion.y, &cropRegion.w, &cropRegion.h);
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

    getMaxYuvSize(&maxYuvW, &maxYuvH);

    if ((getCameraId() == CAMERA_ID_BACK_0)
#ifdef USE_DUAL_CAMERA
        || (getCameraId() == CAMERA_ID_BACK_1)
#endif
       ) {
        if (maxYuvW > UHD_DVFS_CEILING_WIDTH || maxYuvH > UHD_DVFS_CEILING_HEIGHT) {
            hwBdsW = sizeList[BCROP_W];
            hwBdsH = sizeList[BCROP_H];
            setUseFullSizeLUT(true);
        } else if (maxYuvW > sizeList[BDS_W] || maxYuvH > sizeList[BDS_H]) {
            hwBdsW = UHD_DVFS_CEILING_WIDTH;
            hwBdsH = UHD_DVFS_CEILING_HEIGHT;
            setUseFullSizeLUT(true);
        }
    } else {
        if (maxYuvW > sizeList[BDS_W] || maxYuvH > sizeList[BDS_H]) {
            hwBdsW = sizeList[BCROP_W];
            hwBdsH = sizeList[BCROP_H];
            setUseFullSizeLUT(true);
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
    int curRatio = SIZE_RATIO_16_9;
    bool ret = true;

    getYuvSize(&curYuvWidth, &curYuvHeight, outputPortId);

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
        m_setYuvSize(width, height, outputPortId);
    }

    if (outputPortId < YUV_MAX) {
        /* Update minimum YUV size */
        if(m_cameraInfo.minYuvW == 0) {
            m_cameraInfo.minYuvW = width;
        } else if (width < m_cameraInfo.minYuvW) {
            m_cameraInfo.minYuvW = width;
        }

        if(m_cameraInfo.minYuvH == 0) {
            m_cameraInfo.minYuvH = height;
        } else if (height < m_cameraInfo.minYuvH) {
            m_cameraInfo.minYuvH = height;
        }

        /* Update maximum YUV size */
        if(m_cameraInfo.maxYuvW == 0) {
            m_cameraInfo.maxYuvW = width;
            m_cameraInfo.yuvSizeRatioId = curRatio;
        } else if (width > m_cameraInfo.maxYuvW) {
            m_cameraInfo.maxYuvW = width;
            m_cameraInfo.yuvSizeRatioId = curRatio;
        }

        if(m_cameraInfo.maxYuvH == 0) {
            m_cameraInfo.maxYuvH = height;
            m_cameraInfo.yuvSizeRatioId = curRatio;
        } else if (height > m_cameraInfo.maxYuvH) {
            m_cameraInfo.maxYuvH = height;
            m_cameraInfo.yuvSizeRatioId = curRatio;
        }

         CLOGV("m_minYuvW(%d) m_minYuvH(%d) m_maxYuvW(%d) m_maxYuvH(%d)",
            m_cameraInfo.minYuvW, m_cameraInfo.minYuvH, m_cameraInfo.maxYuvW, m_cameraInfo.maxYuvH);
    }

    return NO_ERROR;
}

status_t ExynosCameraParameters::checkHwYuvSize(const int hwWidth, const int hwHeight, const int outputPortId, bool reprocessing)
{
    int curHwYuvWidth = 0;
    int curHwYuvHeight = 0;
    bool ret = true;

    getHwYuvSize(&curHwYuvWidth, &curHwYuvHeight, outputPortId);

    CLOGI("curHwYuvSize %dx%d newHwYuvSize %dx%d outputPortId %d",
            curHwYuvWidth, curHwYuvHeight, hwWidth, hwHeight, outputPortId);

    if (curHwYuvWidth != hwWidth || curHwYuvHeight != hwHeight) {
        m_setHwYuvSize(hwWidth, hwHeight, outputPortId);
    }

    if (outputPortId < YUV_MAX) {
        /* Update maximum YUV size */
        if(m_cameraInfo.maxHwYuvW == 0) {
            m_cameraInfo.maxHwYuvW = hwWidth;
        } else if (hwWidth > m_cameraInfo.maxHwYuvW) {
            m_cameraInfo.maxHwYuvW = hwWidth;
        }

        if(m_cameraInfo.maxHwYuvH == 0) {
            m_cameraInfo.maxHwYuvH = hwHeight;
        } else if (hwHeight > m_cameraInfo.maxHwYuvH) {
            m_cameraInfo.maxHwYuvH = hwHeight;
        }

         CLOGV("m_maxYuvW(%d) m_maxYuvH(%d)", m_cameraInfo.maxHwYuvW, m_cameraInfo.maxHwYuvH);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::getYuvVendorSize(int *width, int *height, int index, ExynosRect ispSize)
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
        && (getYuvStallPort() == (index % ExynosCameraParameters::YUV_MAX))) {
        if (getYuvStallPortUsage() == YUV_STALL_USAGE_DSCALED) {
            getDScaledYuvStallSize(width, height);
        } else {
            *width = m_cameraInfo.hwYuvWidth[index];
            *height = m_cameraInfo.hwYuvHeight[index];
        }
    } else {
        *width = m_cameraInfo.hwYuvWidth[index];
        *height = m_cameraInfo.hwYuvHeight[index];
    }
}

void ExynosCameraParameters::setThumbnailCbSize(int w, int h)
{
    m_thumbnailCbW = w;
    m_thumbnailCbH = h;
}

void ExynosCameraParameters::getThumbnailCbSize(int *w, int *h)
{
    *w = m_thumbnailCbW;
    *h = m_thumbnailCbH;
}

void ExynosCameraParameters::resetThumbnailCbSize()
{
    m_thumbnailCbW = 0;
    m_thumbnailCbH = 0;
}

void ExynosCameraParameters::getDScaledYuvStallSize(int *w, int *h)
{
    *w = YUVSTALL_DSCALED_SIZE_W;
    *h = YUVSTALL_DSCALED_SIZE_H;
}

#ifdef SUPPORT_DEPTH_MAP
bool ExynosCameraParameters::isDepthMapSupported(void) {
    if (m_staticInfo->depthMapSizeLut!= NULL) {
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

bool ExynosCameraParameters::getUseDepthMap(void)
{
    CLOGV("  m_flagUseDepthMap(%d)",  m_flagUseDepthMap);

    return m_flagUseDepthMap;
}

void ExynosCameraParameters::setUseDepthMap(bool useDepthMap)
{
    m_flagUseDepthMap = useDepthMap;
}
#endif

void ExynosCameraParameters::m_setExifChangedAttribute(exif_attribute_t *exifInfo,
                                                        ExynosRect       *pictureRect,
                                                        ExynosRect       *thumbnailRect,
                                                        camera2_shot_t   *shot,
                                                        bool                useDebugInfo2)
{
    unsigned int offset = 0;
    debug_attribute_t &debugInfo = ((useDebugInfo2 == false) ? mDebugInfo : mDebugInfo2);

    m_staticInfoExifLock.lock();
    /* Maker Note */
    /* Clear previous debugInfo data */
    memset((void *)debugInfo.debugData[APP_MARKER_4], 0, debugInfo.debugSize[APP_MARKER_4]);
    /* back-up udm info for exif's maker note */
    memcpy((void *)debugInfo.debugData[APP_MARKER_4], (void *)&shot->udm, sizeof(struct camera2_udm));
    offset = sizeof(struct camera2_udm);

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
    exifInfo->iso_speed_rating = shot->udm.internal.vendorSpecific2[101];
#else
    if (shot->dm.sensor.sensitivity < m_staticInfo->sensitivityRange[MIN]) {
        exifInfo->iso_speed_rating = m_staticInfo->sensitivityRange[MIN];
    } else {
        exifInfo->iso_speed_rating = shot->dm.sensor.sensitivity;
    }
#endif

    /* Exposure Program */
    exifInfo->exposure_program = EXIF_DEF_EXPOSURE_PROGRAM;

    /* Exposure Time */
    exifInfo->exposure_time.num = 1;
#if 0 /* TODO: Must be same with the exposure time in Result Metadata */
    if (shot->udm.ae.vendorSpecific[0] == 0xAEAEAEAE) {
        exifInfo->exposure_time.den = (uint32_t) shot->udm.ae.vendorSpecific[64];
    } else
#endif
    {
        /* HACK : Sometimes, F/W does NOT send the exposureTime */
        if (shot->dm.sensor.exposureTime != 0)
            exifInfo->exposure_time.den = (uint32_t)round((double)1e9 / shot->dm.sensor.exposureTime);
        else
            exifInfo->exposure_time.num = 0;
    }

    /* Shutter Speed */
    exifInfo->shutter_speed.num = (uint32_t) (ROUND_OFF_HALF(((double) (shot->udm.internal.vendorSpecific2[103] / 256.f) * EXIF_DEF_APEX_DEN), 0));
    exifInfo->shutter_speed.den = EXIF_DEF_APEX_DEN;

    /* Aperture */
    uint32_t aperture_num = 0;
    float aperture_f = 0.0f;

    if (m_staticInfo->availableApertureValues == NULL) {
        aperture_num = exifInfo->fnumber.num;
    } else {
        aperture_f = ((float) shot->dm.lens.aperture / 100);
        aperture_num = (uint32_t)(round(aperture_f * COMMON_DENOMINATOR));
    }

    exifInfo->aperture.num = APEX_FNUM_TO_APERTURE((double) (aperture_num) / (double) (exifInfo->fnumber.den)) * COMMON_DENOMINATOR;
    exifInfo->aperture.den = COMMON_DENOMINATOR;

    /* Max Aperture */
    exifInfo->max_aperture.num = APEX_FNUM_TO_APERTURE((double) (aperture_num) / (double) (exifInfo->fnumber.den)) * COMMON_DENOMINATOR;
    exifInfo->max_aperture.den = COMMON_DENOMINATOR;

    /* Brightness */
    int temp = shot->udm.internal.vendorSpecific2[102];
    if ((int) shot->udm.ae.vendorSpecific[102] < 0)
        temp = -temp;
    exifInfo->brightness.num = (int32_t) (ROUND_OFF_HALF((double)((temp * EXIF_DEF_APEX_DEN)/256.f), 0));
    if ((int) shot->udm.ae.vendorSpecific[102] < 0)
        exifInfo->brightness.num = -exifInfo->brightness.num;
    exifInfo->brightness.den = EXIF_DEF_APEX_DEN;

    CLOGD(" udm->internal.vendorSpecific2[100](%d)", shot->udm.internal.vendorSpecific2[100]);
    CLOGD(" udm->internal.vendorSpecific2[101](%d)", shot->udm.internal.vendorSpecific2[101]);
    CLOGD(" udm->internal.vendorSpecific2[102](%d)", shot->udm.internal.vendorSpecific2[102]);
    CLOGD(" udm->internal.vendorSpecific2[103](%d)", shot->udm.internal.vendorSpecific2[103]);

    CLOGD(" iso_speed_rating(%d)", exifInfo->iso_speed_rating);
    CLOGD(" exposure_time(%d/%d)", exifInfo->exposure_time.num, exifInfo->exposure_time.den);
    CLOGD(" shutter_speed(%d/%d)", exifInfo->shutter_speed.num, exifInfo->shutter_speed.den);
    CLOGD(" aperture     (%d/%d)", exifInfo->aperture.num, exifInfo->aperture.den);
    CLOGD(" brightness   (%d/%d)", exifInfo->brightness.num, exifInfo->brightness.den);

    /* Exposure Bias */
    exifInfo->exposure_bias.num = shot->ctl.aa.aeExpCompensation * (m_staticInfo->exposureCompensationStep * 10);
    exifInfo->exposure_bias.den = 10;

    /* Metering Mode */
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

    /* Flash Mode */
    if (shot->ctl.flash.flashMode == CAM2_FLASH_MODE_TORCH) {
        exifInfo->flash = 1;
    } else {
        exifInfo->flash = getMarkingOfExifFlash();
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

    setMarkingOfExifFlash(0);
}

void ExynosCameraParameters::resetYuvStallPort(void)
{
    m_yuvStallPort = -1;
}

void ExynosCameraParameters::setYuvStallPort(int port)
{
    if (port == -1) {
        m_yuvStallPort = ExynosCameraParameters::YUV_2;
    } else {
        m_yuvStallPort = port;
    }

    CLOGI("outputPortId - %d ", m_yuvStallPort);
}

int ExynosCameraParameters::getYuvStallPort(void)
{
    return m_yuvStallPort;
}

void ExynosCameraParameters::setYuvStallPortUsage(int usage)
{
    m_flagYuvStallPortUsage = usage;
}

int ExynosCameraParameters::getYuvStallPortUsage(void)
{
    return m_flagYuvStallPortUsage;
}

int ExynosCameraParameters::getSensorControlDelay()
{
    int sensorRequestDelay = 0;
#ifdef SENSOR_REQUEST_DELAY
    if (this->getVisionMode() == true) {
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

int ExynosCameraParameters::getAntibanding()
{
    return AA_AE_ANTIBANDING_AUTO;
}

status_t ExynosCameraParameters::checkPreviewFpsRange(uint32_t minFps, uint32_t maxFps)
{
    status_t ret = NO_ERROR;
    uint32_t curMinFps = 0, curMaxFps = 0;

    getPreviewFpsRange(&curMinFps, &curMaxFps);

    if (m_adjustPreviewFpsRange(minFps, maxFps) != NO_ERROR) {
        CLOGE("Fails to adjust preview fps range");
        return INVALID_OPERATION;
    }

    if (curMinFps != minFps || curMaxFps != maxFps) {
        int32_t delayed = false;

        if (curMaxFps <= 30 && maxFps == 60) {
            /* 60fps mode */
#ifdef SUPPORT_RESTART_TRANSITION_HIGHSPEED
            delayed = true;
#endif
            this->setHighSpeedMode(CONFIG_MODE::HIGHSPEED_60, delayed);
            this->setRestartStream(true);

        } else if (curMaxFps == 60 && maxFps <= 30) {
            /* 30fps mode */
#ifdef SUPPORT_RESTART_TRANSITION_HIGHSPEED
            delayed = true;
#endif
            this->setHighSpeedMode(CONFIG_MODE::NORMAL, delayed);
            this->setRestartStream(true);
        }

#ifdef USE_DUAL_CAMERA
        /* HACK: Preview fps of Dual camera must fixed */
        m_setPreviewFpsRange(maxFps, maxFps, delayed);
#else
        m_setPreviewFpsRange(minFps, maxFps, delayed);
#endif
#ifdef USE_LIMITATION_FOR_THIRD_PARTY
        if (m_fpsProperty != 0) {
            CLOGI("set to %d fps depends on fps property", m_fpsProperty/1000);
        }
#endif
    }

    return ret;
}

#ifdef SUPPORT_RESTART_TRANSITION_HIGHSPEED
status_t ExynosCameraParameters::updateHWParameter()
{
    status_t ret = NO_ERROR;
    /* 1. config mode */
    int32_t curMode = getConfigMode(false);
    int32_t nextMode = getConfigMode(true);

    if (curMode != nextMode) {
        setHighSpeedMode(nextMode);
        CLOGD("update restart config mode[%d -> %d]", curMode, nextMode);
    }

    /* 2. fps range */
    uint32_t curMinFps = 0, curMaxFps = 0;
    uint32_t nextMinFps = 0, nextMaxFps = 0;

    getPreviewFpsRange(&curMinFps, &curMaxFps, false);
    getPreviewFpsRange(&nextMinFps, &nextMaxFps, true);
    if (curMinFps != nextMinFps || curMaxFps != nextMaxFps) {
        m_setPreviewFpsRange(nextMinFps, nextMinFps);
        CLOGD("update restart fpsRange[%d %d -> %d %d]", curMinFps, curMaxFps, nextMinFps, nextMaxFps);
    }

    return ret;
}
#endif

status_t ExynosCameraParameters::m_adjustPreviewFpsRange(uint32_t &newMinFps, uint32_t &newMaxFps)
{
    bool flagSpecialMode = false;

    if (getPIPMode() == true) {
        flagSpecialMode = true;
        CLOGV(" PIPMode(true), newMaxFps=%d", newMaxFps);
    }

    if (getPIPRecordingHint() == true) {
        flagSpecialMode = true;
        CLOGV(" PIPRecordingHint(true), newMaxFps=%d", newMaxFps);
    }

#ifdef USE_LIMITATION_FOR_THIRD_PARTY
    switch(m_fpsProperty) {
    case 30000:
    case 15000:
        newMinFps = m_fpsProperty / 1000;
        newMaxFps = m_fpsProperty / 1000;
        break;
    default:
        /* Don't use to set fixed fps in the hal side. */
        break;
    }
#endif

    return NO_ERROR;
}

void ExynosCameraParameters::setExposureTime(int64_t exposureTime)
{
    m_exposureTime = exposureTime;
}

int64_t ExynosCameraParameters::getExposureTime(void)
{
    return m_exposureTime;
}

void ExynosCameraParameters::setGain(int gain)
{
    m_gain = gain;
}

int ExynosCameraParameters::getGain(void)
{
    return m_gain;
}

void ExynosCameraParameters::setLedPulseWidth(int64_t ledPulseWidth)
{
    m_ledPulseWidth = ledPulseWidth;
}

int64_t ExynosCameraParameters::getLedPulseWidth(void)
{
    return m_ledPulseWidth;
}

void ExynosCameraParameters::setLedPulseDelay(int64_t ledPulseDelay)
{
    m_ledPulseDelay = ledPulseDelay;
}

int64_t ExynosCameraParameters::getLedPulseDelay(void)
{
    return m_ledPulseDelay;
}

void ExynosCameraParameters::setLedCurrent(int ledCurrent)
{
    m_ledCurrent = ledCurrent;
}

int ExynosCameraParameters::getLedCurrent(void)
{
    return m_ledCurrent;
}

void ExynosCameraParameters::setLedMaxTime(int ledMaxTime)
{
    m_ledMaxTime = ledMaxTime;
}

int ExynosCameraParameters::getLedMaxTime(void)
{
    return m_ledMaxTime;
}

}; /* namespace android */
