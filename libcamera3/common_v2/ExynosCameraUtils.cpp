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
#define LOG_TAG "ExynosCameraUtils"
#include <log/log.h>

#include "ExynosCameraUtils.h"
#include <utils/CallStack.h>

#define ADD_BAYER_BY_NEON

namespace android {

status_t getCropRectAlign(
        int  src_w,  int   src_h,
        int  dst_w,  int   dst_h,
        int *crop_x, int *crop_y,
        int *crop_w, int *crop_h,
        int align_w, int align_h,
        int zoom, float zoomRatio)
{
    *crop_w = src_w;
    *crop_h = src_h;

    if (src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) {
        ALOGE("ERR(%s):width or height values is 0, src(%dx%d), dst(%dx%d)",
                 __func__, src_w, src_h, dst_w, dst_h);
        return BAD_VALUE;
    }

    /* Calculation aspect ratio */
    if (   src_w != dst_w
        || src_h != dst_h) {
        float src_ratio = 1.0f;
        float dst_ratio = 1.0f;

        /* ex : 1024 / 768 */
        src_ratio = ROUND_OFF_HALF(((float)src_w / (float)src_h), 2);

        /* ex : 352  / 288 */
        dst_ratio = ROUND_OFF_HALF(((float)dst_w / (float)dst_h), 2);

        if (dst_ratio <= src_ratio) {
            /* shrink w */
            *crop_w = src_h * ((float)dst_w / (float)dst_h);
            *crop_h = src_h;
        } else {
            /* shrink h */
            *crop_w = src_w;
            *crop_h = src_w / ((float)dst_w / (float)dst_h);
        }
    }

    /* Calculation zoom */
    if (zoom != 0) {
#if defined(SCALER_MAX_SCALE_UP_RATIO)
        /*
         * After dividing float & casting int,
         * zoomed size can be smaller too much.
         * so, when zoom until max, ceil up about floating point.
         */
        if (((int)((float)*crop_w / zoomRatio)) * SCALER_MAX_SCALE_UP_RATIO < *crop_w ||
            ((int)((float)*crop_h / zoomRatio)) * SCALER_MAX_SCALE_UP_RATIO < *crop_h) {
            *crop_w = (int)ceil(((float)*crop_w / zoomRatio));
            *crop_h = (int)ceil(((float)*crop_h / zoomRatio));
        } else
#endif
        {
            *crop_w = (int)((float)*crop_w / zoomRatio);
            *crop_h = (int)((float)*crop_h / zoomRatio);
        }
    } else {
#ifdef SCALER_MAX_SCALE_UP_RATIO
        if (*crop_w * SCALER_MAX_SCALE_UP_RATIO < dst_w
            || *crop_h * SCALER_MAX_SCALE_UP_RATIO < dst_h) {
            int old_crop_w = 0;
            int old_crop_h = 0;
            old_crop_w = *crop_w;
            old_crop_h = *crop_h;
            *crop_w = (int)ceil((float)dst_w / SCALER_MAX_SCALE_UP_RATIO);
            *crop_h = (int)ceil((float)dst_h / SCALER_MAX_SCALE_UP_RATIO);
            ALOGD("DEBUG(%s[%d]):exceed max scaleup ratio : maxRatio(%d) old_crop(%d %d) new_crop(%d %d) dst(%d %d)",
                  __FUNCTION__, __LINE__, SCALER_MAX_SCALE_UP_RATIO, old_crop_w, old_crop_h, *crop_w, *crop_h, dst_w, dst_h);
        }
#endif
    }

    if (dst_w == dst_h) {
        int align_value = 0;
        align_value = (align_w < align_h)? align_h : align_w;

        *crop_w = ALIGN_UP(*crop_w, align_value);
        *crop_h = ALIGN_UP(*crop_h, align_value);

        if (*crop_w > src_w) {
            *crop_w = ALIGN_DOWN(src_w, align_value);
            *crop_h = *crop_w;
        } else if (*crop_h > src_h) {
            *crop_h = ALIGN_DOWN(src_h, align_value);
            *crop_w = *crop_h;
        }
    } else {
        *crop_w = ALIGN_UP(*crop_w, align_w);
        *crop_h = ALIGN_UP(*crop_h, align_h);

        if (*crop_w > src_w)
            *crop_w = ALIGN_DOWN(src_w, align_w);
        if (*crop_h > src_h)
            *crop_h = ALIGN_DOWN(src_h, align_h);
    }

    *crop_x = ALIGN_DOWN(((src_w - *crop_w) >> 1), 2);
    *crop_y = ALIGN_DOWN(((src_h - *crop_h) >> 1), 2);

    if (*crop_x < 0 || *crop_y < 0) {
        ALOGE("ERR(%s):crop size too big (%d, %d, %d, %d)",
                 __func__, *crop_x, *crop_y, *crop_w, *crop_h);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t getCropRectAlign(
        int  src_w,  int   src_h,
        int  dst_w,  int   dst_h,
        int *crop_x, int *crop_y,
        int *crop_w, int *crop_h,
        int align_w, int align_h,
        float zoomRatio)
{
    *crop_w = src_w;
    *crop_h = src_h;

    if (src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) {
        ALOGE("ERR(%s):width or height values is 0, src(%dx%d), dst(%dx%d)",
                 __func__, src_w, src_h, dst_w, dst_h);
        return BAD_VALUE;
    }

    /* Calculation aspect ratio */
    if (   src_w != dst_w
        || src_h != dst_h) {
        float src_ratio = 1.0f;
        float dst_ratio = 1.0f;

        /* ex : 1024 / 768 */
        src_ratio = ROUND_OFF_HALF(((float)src_w / (float)src_h), 2);

        /* ex : 352  / 288 */
        dst_ratio = ROUND_OFF_HALF(((float)dst_w / (float)dst_h), 2);

        if (dst_ratio <= src_ratio) {
            /* shrink w */
            *crop_w = src_h * ((float)dst_w / (float)dst_h);
            *crop_h = src_h;
        } else {
            /* shrink h */
            *crop_w = src_w;
            *crop_h = src_w / ((float)dst_w / (float)dst_h);
        }
    }

    /* Calculation zoom */
    if (zoomRatio != 1.0f) {
#if defined(SCALER_MAX_SCALE_UP_RATIO)
        /*
         * After dividing float & casting int,
         * zoomed size can be smaller too much.
         * so, when zoom until max, ceil up about floating point.
         */
        if (((int)((float)*crop_w / zoomRatio)) * SCALER_MAX_SCALE_UP_RATIO < *crop_w ||
            ((int)((float)*crop_h / zoomRatio)) * SCALER_MAX_SCALE_UP_RATIO < *crop_h) {
            *crop_w = (int)ceil(((float)*crop_w / zoomRatio));
            *crop_h = (int)ceil(((float)*crop_h / zoomRatio));
        } else
#endif
        {
            *crop_w = (int)((float)*crop_w / zoomRatio);
            *crop_h = (int)((float)*crop_h / zoomRatio);
        }
    }

    if (dst_w == dst_h) {
        int align_value = 0;
        align_value = (align_w < align_h)? align_h : align_w;

        *crop_w = ALIGN_UP(*crop_w, align_value);
        *crop_h = ALIGN_UP(*crop_h, align_value);

        if (*crop_w > src_w) {
            *crop_w = ALIGN_DOWN(src_w, align_value);
            *crop_h = *crop_w;
        } else if (*crop_h > src_h) {
            *crop_h = ALIGN_DOWN(src_h, align_value);
            *crop_w = *crop_h;
        }
    } else {
        *crop_w = ALIGN_UP(*crop_w, align_w);
        *crop_h = ALIGN_UP(*crop_h, align_h);

        if (*crop_w > src_w)
            *crop_w = ALIGN_DOWN(src_w, align_w);
        if (*crop_h > src_h)
            *crop_h = ALIGN_DOWN(src_h, align_h);
    }

    *crop_x = ALIGN_DOWN(((src_w - *crop_w) >> 1), 2);
    *crop_y = ALIGN_DOWN(((src_h - *crop_h) >> 1), 2);

    if (*crop_x < 0 || *crop_y < 0) {
        ALOGE("ERR(%s):crop size too big (%d, %d, %d, %d)",
                 __func__, *crop_x, *crop_y, *crop_w, *crop_h);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

uint32_t bracketsStr2Ints(
        char *str,
        uint32_t num,
        ExynosRect2 *rect2s,
        int *weights,
        int mode)
{
    char *curStr = str;
    char buf[128];
    char *bracketsOpen;
    char *bracketsClose;

    int tempArray[5] = {0,};
    uint32_t validFocusedAreas = 0;
    bool isValid;
    bool nullArea = false;
    isValid = true;

    for (uint32_t i = 0; i < num + 1; i++) {
        if (curStr == NULL) {
            if (i != num) {
                nullArea = false;
            }
            break;
        }

        bracketsOpen = strchr(curStr, '(');
        if (bracketsOpen == NULL) {
            if (i != num) {
                nullArea = false;
            }
            break;
        }

        bracketsClose = strchr(bracketsOpen, ')');
        if (bracketsClose == NULL) {
            ALOGE("ERR(%s):subBracketsStr2Ints(%s) fail", __func__, buf);
            if (i != num) {
                nullArea = false;
            }
            break;
        } else if (i == num) {
            return 0;
        }

        strncpy(buf, bracketsOpen, bracketsClose - bracketsOpen + 1);
        buf[bracketsClose - bracketsOpen + 1] = 0;

        if (subBracketsStr2Ints(5, buf, tempArray) == false) {
            nullArea = false;
            break;
        }

        rect2s[i].x1 = tempArray[0];
        rect2s[i].y1 = tempArray[1];
        rect2s[i].x2 = tempArray[2];
        rect2s[i].y2 = tempArray[3];
        weights[i] = tempArray[4];

        if (mode) {
            isValid = true;

            for (int j = 0; j < 4; j++) {
                if (tempArray[j] < -1000 || tempArray[j] > 1000)
                    isValid = false;
            }

            if (tempArray[4] < 0 || tempArray[4] > 1000)
                isValid = false;

            if (!rect2s[i].x1 && !rect2s[i].y1 && !rect2s[i].x2 && !rect2s[i].y2 && !weights[i])
                nullArea = true;
            else if (weights[i] == 0)
                isValid = false;
            else if (!(tempArray[0] == 0 && tempArray[2] == 0) && tempArray[0] >= tempArray[2])
                isValid = false;
            else if (!(tempArray[1] == 0 && tempArray[3] == 0) && tempArray[1] >= tempArray[3])
                isValid = false;
            else if (!(tempArray[0] == 0 && tempArray[2] == 0) && (tempArray[1] == 0 && tempArray[3] == 0))
                isValid = false;
            else if ((tempArray[0] == 0 && tempArray[2] == 0) && !(tempArray[1] == 0 && tempArray[3] == 0))
                isValid = false;

            if (isValid)
                validFocusedAreas++;
            else
                return 0;
        } else {
            if (rect2s[i].x1 || rect2s[i].y1 || rect2s[i].x2 || rect2s[i].y2 || weights[i])
                validFocusedAreas++;
        }

        curStr = bracketsClose;
    }
    if (nullArea && mode)
        validFocusedAreas = num;

    if (validFocusedAreas == 0)
        validFocusedAreas = 1;

    ALOGD("DEBUG(%s[%d]):(%d,%d,%d,%d,%d) - validFocusedAreas(%d)", __FUNCTION__, __LINE__,
            tempArray[0], tempArray[1], tempArray[2], tempArray[3], tempArray[4], validFocusedAreas);

    return validFocusedAreas;
}

bool subBracketsStr2Ints(int num, char *str, int *arr)
{
    if (str == NULL || arr == NULL) {
        ALOGE("ERR(%s):str or arr is NULL", __func__);
        return false;
    }

    /* ex : (-10,-10,0,0,300) */
    char buf[128];
    char *bracketsOpen;
    char *bracketsClose;
    char *tok;
    char *savePtr;

    bracketsOpen = strchr(str, '(');
    if (bracketsOpen == NULL) {
        ALOGE("ERR(%s):no '('", __func__);
        return false;
    }

    bracketsClose = strchr(bracketsOpen, ')');
    if (bracketsClose == NULL) {
        ALOGE("ERR(%s):no ')'", __func__);
        return false;
    }

    strncpy(buf, bracketsOpen + 1, bracketsClose - bracketsOpen + 1);
    buf[bracketsClose - bracketsOpen + 1] = 0;

    tok = strtok_r(buf, ",", &savePtr);
    if (tok == NULL) {
        ALOGE("ERR(%s):strtok_r(%s) fail", __func__, buf);
        return false;
    }

    arr[0] = atoi(tok);

    for (int i = 1; i < num; i++) {
        tok = strtok_r(NULL, ",", &savePtr);
        if (tok == NULL) {
            if (i < num - 1) {
                ALOGE("ERR(%s):strtok_r() (index : %d, num : %d) fail", __func__, i, num);
                return false;
            }
            break;
        }

        arr[i] = atoi(tok);
    }

    return true;
}

void convertingRectToRect2(ExynosRect *rect, ExynosRect2 *rect2)
{
    rect2->x1 = rect->x;
    rect2->y1 = rect->y;
    rect2->x2 = rect->x + rect->w - 1;
    rect2->y2 = rect->y + rect->h - 1;
}

void convertingRect2ToRect(ExynosRect2 *rect2, ExynosRect *rect)
{
    rect->x = rect2->x1;
    rect->y = rect2->y1;
    rect->w = rect2->x2 - rect2->x1 + 1;
    rect->h = rect2->y2 - rect2->y1 + 1;
}

bool isRectNull(ExynosRect *rect)
{
    if (   rect->x == 0
        && rect->y == 0
        && rect-> w == 0
        && rect->h == 0
        && rect->fullW == 0
        && rect->fullH == 0
        && rect->colorFormat == 0)
        return true;

    return false;
}

bool isRectNull(ExynosRect2 *rect2)
{
    if (   rect2->x1 == 0
        && rect2->y1 == 0
        && rect2->x2 == 0
        && rect2->y2 == 0)
        return true;

    return false;
}

bool isRectEqual(ExynosRect *rect1, ExynosRect *rect2)
{
    if (   rect1->x == rect2->x
        && rect1->y == rect2->y
        && rect1->w == rect2->w
        && rect1->h == rect2->h
        && rect1->fullW == rect2->fullW
        && rect1->fullH == rect2->fullH
        && rect1->colorFormat == rect2->colorFormat)
        return true;

    return false;
}

bool isRectEqual(ExynosRect2 *rect1, ExynosRect2 *rect2)
{
    if (   rect1->x1 == rect2->x1
        && rect1->y1 == rect2->y1
        && rect1->x2 == rect2->x2
        && rect1->y2 == rect2->y2)
        return true;

    return false;
}

char v4l2Format2Char(int v4l2Format, int pos)
{
    char c;

    switch (pos) {
    case 0:
        c = (char)((v4l2Format >>  0) & 0xFF);
        break;
    case 1:
        c = (char)((v4l2Format >> 8) & 0xFF);
        break;
    case 2:
        c = (char)((v4l2Format >> 16) & 0xFF);
        break;
    case 3:
        c = (char)((v4l2Format >> 24) & 0xFF);
        break;
    default:
        c = ' ';
        break;
    }

    return c;
}

ExynosRect2 convertingAndroidArea2HWArea(ExynosRect2 *srcRect, const ExynosRect *regionRect)
{
    int w = regionRect->w;
    int h = regionRect->h;

    ExynosRect2 newRect2;

    newRect2.x1 = (srcRect->x1 + 1000) * w / 2000;
    newRect2.y1 = (srcRect->y1 + 1000) * h / 2000;
    newRect2.x2 = (srcRect->x2 + 1000) * w / 2000;
    newRect2.y2 = (srcRect->y2 + 1000) * h / 2000;

    if (newRect2.x1 < 0)
        newRect2.x1 = 0;
    else if (w <= newRect2.x1)
        newRect2.x1 = w - 1;

    if (newRect2.y1 < 0)
        newRect2.y1 = 0;
    else if (h <= newRect2.y1)
        newRect2.y1 = h - 1;

    if (newRect2.x2 < 0)
        newRect2.x2 = 0;
    else if (w <= newRect2.x2)
        newRect2.x2 = w - 1;

    if (newRect2.y2 < 0)
        newRect2.y2 = 0;
    else if (h <= newRect2.y2)
        newRect2.y2 = h - 1;

    if (newRect2.x2 < newRect2.x1)
        newRect2.x2 = newRect2.x1;

    if (newRect2.y2 < newRect2.y1)
        newRect2.y2 = newRect2.y1;

    ALOGV("INFO(%s[%d]): src(%d %d %d %d) region(%d %d %d %d) newRect(%d %d %d %d)", __FUNCTION__, __LINE__,
        srcRect->x1, srcRect->y1, srcRect->x2, srcRect->y2,
        regionRect->x , regionRect->y, regionRect->w, regionRect->h,
        newRect2.x1 , newRect2.y1, newRect2.x2, newRect2.y2);

    return newRect2;
}

ExynosRect2 convertingAndroidArea2HWAreaBcropOut(ExynosRect2 *srcRect, const ExynosRect *regionRect)
{
    /* do nothing, same as noraml converting */
    return convertingAndroidArea2HWArea(srcRect, regionRect);
}

ExynosRect2 convertingHWArea2AndroidArea(ExynosRect2 *srcRect, ExynosRect *regionRect)
{
    int w = regionRect->w;
    int h = regionRect->h;

    int min_val = -1000;
    int max_val = 1000;

    ExynosRect2 newRect2;

    newRect2.x1 = ((srcRect->x1 * 2000) / w) - 1000;
    newRect2.y1 = ((srcRect->y1 * 2000) / h) - 1000;
    newRect2.x2 = ((srcRect->x2 * 2000) / w) - 1000;
    newRect2.y2 = ((srcRect->y2 * 2000) / h) - 1000;

    if (newRect2.x1 < min_val)
        newRect2.x1 = min_val;
    else if (max_val < newRect2.x1)
        newRect2.x1 = max_val;

    if (newRect2.y1 < min_val)
        newRect2.y1 = min_val;
    else if (max_val < newRect2.y1)
        newRect2.y1 = max_val;

    if (newRect2.x2 < min_val)
        newRect2.x2 = min_val;
    else if (max_val < newRect2.x2)
        newRect2.x2 = max_val;

    if (newRect2.y2 < min_val)
        newRect2.y2 = min_val;
    else if (max_val < newRect2.y2)
        newRect2.y2 = max_val;

    if (newRect2.x2 < newRect2.x1)
        newRect2.x2 = newRect2.x1;

    if (newRect2.y2 < newRect2.y1)
        newRect2.y2 = newRect2.y1;

    ALOGV("INFO(%s[%d]): src(%d %d %d %d) region(%d %d %d %d) newRect(%d %d %d %d)", __FUNCTION__, __LINE__,
        srcRect->x1, srcRect->y1, srcRect->x2, srcRect->y2,
        regionRect->x , regionRect->y, regionRect->w, regionRect->h,
        newRect2.x1 , newRect2.y1, newRect2.x2, newRect2.y2);

    return newRect2;
}

ExynosRect convertingBufferDst2Rect(ExynosCameraBuffer *buffer, int colorFormat)
{
    ExynosRect rect;

    camera2_stream *shot_stream = (camera2_stream *)buffer->addr[buffer->getMetaPlaneIndex()];
    if (shot_stream == NULL) {
        // assert? instead of ALOGE?
        ALOGE("ERR(%s[%d]):shot_stream == NULL. so fail. just set 0", __FUNCTION__, __LINE__);
    } else {
        rect.x     = shot_stream->output_crop_region[0];
        rect.y     = shot_stream->output_crop_region[1];
        rect.w     = shot_stream->output_crop_region[2];
        rect.h     = shot_stream->output_crop_region[3];
        rect.fullW = shot_stream->output_crop_region[2];
        rect.fullH = shot_stream->output_crop_region[3];
        rect.colorFormat = colorFormat;
    }

    return rect;
}

int32_t getMetaDmRequestFrameCount(struct camera2_shot_ext *shot_ext)
{
    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]): buffer is NULL", __FUNCTION__, __LINE__);
        return -1;
    }
    return shot_ext->shot.dm.request.frameCount;
}

int32_t getMetaDmRequestFrameCount(struct camera2_dm *dm)
{
    if (dm == NULL) {
        ALOGE("ERR(%s[%d]): buffer is NULL", __FUNCTION__, __LINE__);
        return -1;
    }
    return dm->request.frameCount;
}

void setMetaCtlAeTargetFpsRange(struct camera2_shot_ext *shot_ext, uint32_t min, uint32_t max)
{
    if (shot_ext->shot.ctl.aa.aeTargetFpsRange[0] != min ||
            shot_ext->shot.ctl.aa.aeTargetFpsRange[1] != max) {
        ALOGI("INFO(%s):aeTargetFpsRange(min=%d, min=%d)", __FUNCTION__, min, max);
    }
    shot_ext->shot.ctl.aa.aeTargetFpsRange[0] = min;
    shot_ext->shot.ctl.aa.aeTargetFpsRange[1] = max;
}

void getMetaCtlAeTargetFpsRange(struct camera2_shot_ext *shot_ext, uint32_t *min, uint32_t *max)
{
    *min = shot_ext->shot.ctl.aa.aeTargetFpsRange[0];
    *max = shot_ext->shot.ctl.aa.aeTargetFpsRange[1];
}

void setMetaCtlSensorFrameDuration(struct camera2_shot_ext *shot_ext, uint64_t duration)
{
    shot_ext->shot.ctl.sensor.frameDuration = duration;
}

void getMetaCtlSensorFrameDuration(struct camera2_shot_ext *shot_ext, uint64_t *duration)
{
    *duration = shot_ext->shot.ctl.sensor.frameDuration;
}

void setMetaCtlAeMode(struct camera2_shot_ext *shot_ext, enum aa_aemode aeMode)
{
    if (shot_ext->shot.ctl.sensor.exposureTime == 0)
        shot_ext->shot.ctl.aa.aeMode = aeMode;
}

void getMetaCtlAeMode(struct camera2_shot_ext *shot_ext, enum aa_aemode *aeMode)
{
    *aeMode = shot_ext->shot.ctl.aa.aeMode;
}

void setMetaCtlAeLock(struct camera2_shot_ext *shot_ext, bool lock)
{
    if (lock == true)
        shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
    else
        shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_OFF;
}

void getMetaCtlAeLock(struct camera2_shot_ext *shot_ext, bool *lock)
{
    if (shot_ext->shot.ctl.aa.aeLock == AA_AE_LOCK_OFF)
        *lock = false;
    else
        *lock = true;
}

#ifdef USE_SUBDIVIDED_EV
void setMetaCtlExposureCompensationStep(struct camera2_shot_ext *shot_ext, float expCompensationStep)
{
    shot_ext->shot.ctl.aa.vendor_aeExpCompensationStep = expCompensationStep;
}
#endif

void setMetaCtlExposureCompensation(struct camera2_shot_ext *shot_ext, int32_t expCompensation)
{
    shot_ext->shot.ctl.aa.aeExpCompensation = expCompensation;
}

void getMetaCtlExposureCompensation(struct camera2_shot_ext *shot_ext, int32_t *expCompensation)
{
    *expCompensation = shot_ext->shot.ctl.aa.aeExpCompensation;
}

void setMetaCtlExposureTime(struct camera2_shot_ext *shot_ext, uint64_t exposureTime)
{
    if (exposureTime != 0) {
        shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_OFF;
        setMetaCtlAeRegion(shot_ext, 0, 0, 0, 0, 0);
    }
    shot_ext->shot.ctl.sensor.exposureTime = exposureTime;
}

void getMetaCtlExposureTime(struct camera2_shot_ext *shot_ext, uint64_t *exposureTime)
{
    *exposureTime = shot_ext->shot.ctl.sensor.exposureTime;
}

void setMetaCtlCaptureExposureTime(struct camera2_shot_ext *shot_ext, uint32_t exposureTime)
{
    shot_ext->shot.ctl.aa.vendor_captureExposureTime = exposureTime;
}

void getMetaCtlCaptureExposureTime(struct camera2_shot_ext *shot_ext, uint32_t *exposureTime)
{
    *exposureTime = shot_ext->shot.ctl.aa.vendor_captureExposureTime;
}

#ifdef SUPPORT_DEPTH_MAP
void setMetaCtlDisparityMode(struct camera2_shot_ext *shot_ext, enum camera2_disparity_mode disparity_mode)
{
    shot_ext->shot.uctl.isModeUd.disparity_mode = disparity_mode;
}
#endif

void setMetaCtlWbLevel(struct camera2_shot_ext *shot_ext, int32_t wbLevel)
{
    shot_ext->shot.ctl.aa.vendor_awbValue = wbLevel;
}

void getMetaCtlWbLevel(struct camera2_shot_ext *shot_ext, int32_t *wbLevel)
{
    *wbLevel = shot_ext->shot.ctl.aa.vendor_awbValue;
}

#ifdef USE_FW_ZOOMRATIO
void setMetaCtlZoom(struct camera2_shot_ext *shot_ext, float data)
{
    shot_ext->shot.uctl.zoomRatio = data;
}
void getMetaCtlZoom(struct camera2_shot_ext *shot_ext, float *data)
{
    *data = shot_ext->shot.uctl.zoomRatio;
}
#endif

status_t setMetaCtlCropRegion(
        struct camera2_shot_ext *shot_ext,
        int x, int y, int w, int h)
{
    shot_ext->shot.ctl.scaler.cropRegion[0] = x;
    shot_ext->shot.ctl.scaler.cropRegion[1] = y;
    shot_ext->shot.ctl.scaler.cropRegion[2] = w;
    shot_ext->shot.ctl.scaler.cropRegion[3] = h;

    return NO_ERROR;
}
status_t setMetaCtlCropRegion(
        struct camera2_shot_ext *shot_ext,
        int zoom,
        int srcW, int srcH,
        int dstW, int dstH, float zoomRatio)
{
    int newX = 0;
    int newY = 0;
    int newW = 0;
    int newH = 0;

    if (getCropRectAlign(srcW,  srcH,
                         dstW,  dstH,
                         &newX, &newY,
                         &newW, &newH,
                         16, 2,
                         zoom, zoomRatio) != NO_ERROR) {
        ALOGE("ERR(%s):getCropRectAlign(%d, %d, %d, %d) fail",
            __func__, srcW,  srcH, dstW,  dstH);
        return BAD_VALUE;
    }

    newX = ALIGN(newX, 2);
    newY = ALIGN(newY, 2);

    ALOGV("DEBUG(%s):size(%d, %d, %d, %d), level(%d)",
        __FUNCTION__, newX, newY, newW, newH, zoom);

    shot_ext->shot.ctl.scaler.cropRegion[0] = newX;
    shot_ext->shot.ctl.scaler.cropRegion[1] = newY;
    shot_ext->shot.ctl.scaler.cropRegion[2] = newW;
    shot_ext->shot.ctl.scaler.cropRegion[3] = newH;

    return NO_ERROR;
}

void getMetaCtlCropRegion(
        struct camera2_shot_ext *shot_ext,
        int *x, int *y,
        int *w, int *h)
{
    *x = shot_ext->shot.ctl.scaler.cropRegion[0];
    *y = shot_ext->shot.ctl.scaler.cropRegion[1];
    *w = shot_ext->shot.ctl.scaler.cropRegion[2];
    *h = shot_ext->shot.ctl.scaler.cropRegion[3];
}

void setMetaCtlAeRegion(
        struct camera2_shot_ext *shot_ext,
        int x, int y,
        int w, int h,
        int weight)
{
    shot_ext->shot.ctl.aa.aeRegions[0] = x;
    shot_ext->shot.ctl.aa.aeRegions[1] = y;
    shot_ext->shot.ctl.aa.aeRegions[2] = w;
    shot_ext->shot.ctl.aa.aeRegions[3] = h;
    shot_ext->shot.ctl.aa.aeRegions[4] = weight;
}

void getMetaCtlAeRegion(
        struct camera2_shot_ext *shot_ext,
        int *x, int *y,
        int *w, int *h,
        int *weight)
{
    *x = shot_ext->shot.ctl.aa.aeRegions[0];
    *y = shot_ext->shot.ctl.aa.aeRegions[1];
    *w = shot_ext->shot.ctl.aa.aeRegions[2];
    *h = shot_ext->shot.ctl.aa.aeRegions[3];
    *weight = shot_ext->shot.ctl.aa.aeRegions[4];
}


void setMetaCtlAntibandingMode(struct camera2_shot_ext *shot_ext, enum aa_ae_antibanding_mode antibandingMode)
{
    shot_ext->shot.ctl.aa.aeAntibandingMode = antibandingMode;
}

void getMetaCtlAntibandingMode(struct camera2_shot_ext *shot_ext, enum aa_ae_antibanding_mode *antibandingMode)
{
    *antibandingMode = shot_ext->shot.ctl.aa.aeAntibandingMode;
}

void setMetaCtlSceneMode(struct camera2_shot_ext *shot_ext, enum aa_mode mode, enum aa_scene_mode sceneMode)
{
    enum processing_mode default_edge_mode = PROCESSING_MODE_FAST;
    enum processing_mode default_noise_mode = PROCESSING_MODE_FAST;
    int default_edge_strength = 5;
    int default_noise_strength = 5;

    shot_ext->shot.ctl.aa.mode = mode;
    shot_ext->shot.ctl.aa.sceneMode = sceneMode;

    switch (sceneMode) {
    case AA_SCENE_MODE_FACE_PRIORITY:
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF
            && shot_ext->shot.ctl.sensor.exposureTime == 0)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.sceneMode = AA_SCENE_MODE_DISABLED;
        if(shot_ext->shot.ctl.aa.vendor_isoMode != AA_ISOMODE_MANUAL) {
            shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
            shot_ext->shot.ctl.aa.vendor_isoValue = 0;
            shot_ext->shot.ctl.sensor.sensitivity = 0;
        }

        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_ACTION:
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_PORTRAIT:
    case AA_SCENE_MODE_LANDSCAPE:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_NIGHT:
        /* AE_LOCK is prohibited */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF ||
            shot_ext->shot.ctl.aa.aeLock == AA_AE_LOCK_ON) {
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
        }

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_NIGHT_PORTRAIT:
    case AA_SCENE_MODE_THEATRE:
    case AA_SCENE_MODE_BEACH:
    case AA_SCENE_MODE_SNOW:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_SUNSET:
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_DAYLIGHT;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_STEADYPHOTO:
    case AA_SCENE_MODE_FIREWORKS:
    case AA_SCENE_MODE_SPORTS:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_PARTY:
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_MANUAL;
        shot_ext->shot.ctl.aa.vendor_isoValue = 200;
        shot_ext->shot.ctl.sensor.sensitivity = 200;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 4; /* "4" is default + 1. */
        break;
    case AA_SCENE_MODE_CANDLELIGHT:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
#ifdef SAMSUNG_FOOD_MODE
    case AA_SCENE_MODE_FOOD:
        shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_MATRIX;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
#endif
    case AA_SCENE_MODE_AQUA:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    default:
        break;
    }
}

void getMetaCtlSceneMode(struct camera2_shot_ext *shot_ext, enum aa_mode *mode, enum aa_scene_mode *sceneMode)
{
    *mode = shot_ext->shot.ctl.aa.mode;
    *sceneMode = shot_ext->shot.ctl.aa.sceneMode;
}

void setMetaCtlAwbMode(struct camera2_shot_ext *shot_ext, enum aa_awbmode awbMode)
{
    shot_ext->shot.ctl.aa.awbMode = awbMode;
}

void getMetaCtlAwbMode(struct camera2_shot_ext *shot_ext, enum aa_awbmode *awbMode)
{
    *awbMode = shot_ext->shot.ctl.aa.awbMode;
}

void setMetaCtlAwbLock(struct camera2_shot_ext *shot_ext, bool lock)
{
    if (lock == true)
        shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
    else
        shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_OFF;
}

void getMetaCtlAwbLock(struct camera2_shot_ext *shot_ext, bool *lock)
{
    if (shot_ext->shot.ctl.aa.awbLock == AA_AWB_LOCK_OFF)
        *lock = false;
    else
        *lock = true;
}

void setMetaVtMode(struct camera2_shot_ext *shot_ext, enum camera_vt_mode mode)
{
    shot_ext->shot.uctl.vtMode = mode;
}

void setMetaVideoMode(struct camera2_shot_ext *shot_ext, enum aa_videomode mode)
{
    shot_ext->shot.ctl.aa.vendor_videoMode = mode;
}

void setMetaCtlAfRegion(struct camera2_shot_ext *shot_ext,
                        int x, int y, int w, int h, int weight)
{
    shot_ext->shot.ctl.aa.afRegions[0] = x;
    shot_ext->shot.ctl.aa.afRegions[1] = y;
    shot_ext->shot.ctl.aa.afRegions[2] = w;
    shot_ext->shot.ctl.aa.afRegions[3] = h;
    shot_ext->shot.ctl.aa.afRegions[4] = weight;
}

void getMetaCtlAfRegion(struct camera2_shot_ext *shot_ext,
                        int *x, int *y, int *w, int *h, int *weight)
{
    *x = shot_ext->shot.ctl.aa.afRegions[0];
    *y = shot_ext->shot.ctl.aa.afRegions[1];
    *w = shot_ext->shot.ctl.aa.afRegions[2];
    *h = shot_ext->shot.ctl.aa.afRegions[3];
    *weight = shot_ext->shot.ctl.aa.afRegions[4];
}

void setMetaCtlColorCorrectionMode(struct camera2_shot_ext *shot_ext, enum colorcorrection_mode mode)
{
    shot_ext->shot.ctl.color.mode = mode;
}

void getMetaCtlColorCorrectionMode(struct camera2_shot_ext *shot_ext, enum colorcorrection_mode *mode)
{
    *mode = shot_ext->shot.ctl.color.mode;
}

void setMetaCtlAaEffect(struct camera2_shot_ext *shot_ext, aa_effect_mode_t mode)
{
    shot_ext->shot.ctl.aa.effectMode = mode;
}

void getMetaCtlAaEffect(struct camera2_shot_ext *shot_ext, aa_effect_mode_t *mode)
{
    *mode = shot_ext->shot.ctl.aa.effectMode;
}

void setMetaCtlBrightness(struct camera2_shot_ext *shot_ext, int32_t brightness)
{
    shot_ext->shot.ctl.color.vendor_brightness = brightness;
}

void getMetaCtlBrightness(struct camera2_shot_ext *shot_ext, int32_t *brightness)
{
    *brightness = shot_ext->shot.ctl.color.vendor_brightness;
}

void setMetaCtlSaturation(struct camera2_shot_ext *shot_ext, int32_t saturation)
{
    shot_ext->shot.ctl.color.vendor_saturation = saturation;
}

void getMetaCtlSaturation(struct camera2_shot_ext *shot_ext, int32_t *saturation)
{
    *saturation = shot_ext->shot.ctl.color.vendor_saturation;
}

void setMetaCtlHue(struct camera2_shot_ext *shot_ext, int32_t hue)
{
    shot_ext->shot.ctl.color.vendor_hue = hue;
}

void getMetaCtlHue(struct camera2_shot_ext *shot_ext, int32_t *hue)
{
    *hue = shot_ext->shot.ctl.color.vendor_hue;
}

void setMetaCtlContrast(struct camera2_shot_ext *shot_ext, uint32_t contrast)
{
    shot_ext->shot.ctl.color.vendor_contrast = contrast;
}

void getMetaCtlContrast(struct camera2_shot_ext *shot_ext, uint32_t *contrast)
{
    *contrast = shot_ext->shot.ctl.color.vendor_contrast;
}

void setMetaCtlSharpness(struct camera2_shot_ext *shot_ext, enum processing_mode edge_mode, int32_t edge_sharpness,
                         enum processing_mode noise_mode, int32_t noise_sharpness)
{
    shot_ext->shot.ctl.edge.mode = edge_mode;
    shot_ext->shot.ctl.edge.strength = (uint8_t) edge_sharpness;
    shot_ext->shot.ctl.noise.mode = noise_mode;
    shot_ext->shot.ctl.noise.strength = (uint8_t) noise_sharpness;
}

void getMetaCtlSharpness(struct camera2_shot_ext *shot_ext, enum processing_mode *edge_mode, int32_t *edge_sharpness,
                         enum processing_mode *noise_mode, int32_t *noise_sharpness)
{
    *edge_mode = shot_ext->shot.ctl.edge.mode;
    *edge_sharpness = (int32_t) shot_ext->shot.ctl.edge.strength;
    *noise_mode = shot_ext->shot.ctl.noise.mode;
    *noise_sharpness = (int32_t) shot_ext->shot.ctl.noise.strength;
}

void setMetaCtlIso(struct camera2_shot_ext *shot_ext, enum aa_isomode mode, uint32_t iso)
{
    shot_ext->shot.ctl.aa.vendor_isoMode = mode;
    shot_ext->shot.ctl.aa.vendor_isoValue = iso;
    shot_ext->shot.ctl.sensor.sensitivity = iso;
}

void getMetaCtlIso(struct camera2_shot_ext *shot_ext, enum aa_isomode *mode, uint32_t *iso)
{
    *mode = shot_ext->shot.ctl.aa.vendor_isoMode;
    *iso = shot_ext->shot.ctl.aa.vendor_isoValue;
}

void setMetaCtlFdMode(struct camera2_shot_ext *shot_ext, enum facedetect_mode mode)
{
    shot_ext->shot.ctl.stats.faceDetectMode = mode;
}

void setMetaUctlYsumPort(struct camera2_shot_ext *shot_ext, enum mcsc_port ysumPort)
{
    shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_YSUM] = ysumPort;
}

void getStreamFrameValid(struct camera2_stream *shot_stream, uint32_t *fvalid)
{
    *fvalid = shot_stream->fvalid;
}

void getStreamFrameCount(struct camera2_stream *shot_stream, uint32_t *fcount)
{
    *fcount = shot_stream->fcount;
}

status_t setMetaDmSensorTimeStamp(struct camera2_shot_ext *shot_ext, uint64_t timeStamp)
{
    status_t status = NO_ERROR;
    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):buffer is NULL", __FUNCTION__, __LINE__);
        status = INVALID_OPERATION;
        return status;
    }

    shot_ext->shot.dm.sensor.timeStamp = timeStamp;
    return status;
}

nsecs_t getMetaDmSensorTimeStamp(struct camera2_shot_ext *shot_ext)
{
    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):buffer is NULL", __FUNCTION__, __LINE__);
        return 0;
    }
    return shot_ext->shot.dm.sensor.timeStamp;
}

status_t setMetaUdmSensorTimeStampBoot(struct camera2_shot_ext *shot_ext, uint64_t timeStamp)
{
    status_t status = NO_ERROR;
    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):buffer is NULL", __FUNCTION__, __LINE__);
        status = INVALID_OPERATION;
        return status;
    }

    shot_ext->shot.udm.sensor.timeStampBoot = timeStamp;
    return status;
}

nsecs_t getMetaUdmSensorTimeStampBoot(struct camera2_shot_ext *shot_ext)
{
    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):buffer is NULL", __FUNCTION__, __LINE__);
        return 0;
    }
    return shot_ext->shot.udm.sensor.timeStampBoot;
}

void setMetaNodeLeaderRequest(struct camera2_shot_ext* shot_ext, int value)
{
    shot_ext->node_group.leader.request = value;

    ALOGV("INFO(%s[%d]):(%d)", __FUNCTION__, __LINE__,
        shot_ext->node_group.leader.request);
}

void setMetaNodeLeaderVideoID(struct camera2_shot_ext* shot_ext, int value)
{
    shot_ext->node_group.leader.vid = value;

    ALOGV("INFO(%s[%d]):(%d)", __FUNCTION__, __LINE__,
    shot_ext->node_group.leader.vid);
}

void setMetaNodeLeaderPixFormat(struct camera2_shot_ext* shot_ext, int value)
{
    shot_ext->node_group.leader.pixelformat = value;

    ALOGV("INFO(%s[%d]):(%d)", __FUNCTION__, __LINE__,
    shot_ext->node_group.leader.pixelformat);
}

void setMetaNodeLeaderInputSize(struct camera2_shot_ext* shot_ext, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
    shot_ext->node_group.leader.input.cropRegion[0] = x;
    shot_ext->node_group.leader.input.cropRegion[1] = y;
    shot_ext->node_group.leader.input.cropRegion[2] = w;
    shot_ext->node_group.leader.input.cropRegion[3] = h;

    ALOGV("INFO(%s[%d]):(%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        shot_ext->node_group.leader.input.cropRegion[0],
        shot_ext->node_group.leader.input.cropRegion[1],
        shot_ext->node_group.leader.input.cropRegion[2],
        shot_ext->node_group.leader.input.cropRegion[3]);
}

void setMetaNodeLeaderOutputSize(struct camera2_shot_ext * shot_ext, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
    shot_ext->node_group.leader.output.cropRegion[0] = x;
    shot_ext->node_group.leader.output.cropRegion[1] = y;
    shot_ext->node_group.leader.output.cropRegion[2] = w;
    shot_ext->node_group.leader.output.cropRegion[3] = h;

    ALOGV("INFO(%s[%d]):(%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        shot_ext->node_group.leader.output.cropRegion[0],
        shot_ext->node_group.leader.output.cropRegion[1],
        shot_ext->node_group.leader.output.cropRegion[2],
        shot_ext->node_group.leader.output.cropRegion[3]);
}

void setMetaNodeCaptureRequest(struct camera2_shot_ext* shot_ext, int index, int value)
{
    shot_ext->node_group.capture[index].request = value;

    ALOGV("INFO(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__,
        index,
        shot_ext->node_group.capture[index].request);
}

void setMetaNodeCaptureVideoID(struct camera2_shot_ext* shot_ext, int index, int value)
{
    shot_ext->node_group.capture[index].vid = value;

    ALOGV("INFO(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__,
        index,
        shot_ext->node_group.capture[index].vid);
}

void setMetaNodeCapturePixFormat(struct camera2_shot_ext* shot_ext, int index, int value)
{
    shot_ext->node_group.capture[index].pixelformat = value;

    ALOGV("INFO(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__,
        index,
        shot_ext->node_group.capture[index].pixelformat);
}

void setMetaNodeCaptureInputSize(struct camera2_shot_ext* shot_ext, int index, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
    shot_ext->node_group.capture[index].input.cropRegion[0] = x;
    shot_ext->node_group.capture[index].input.cropRegion[1] = y;
    shot_ext->node_group.capture[index].input.cropRegion[2] = w;
    shot_ext->node_group.capture[index].input.cropRegion[3] = h;

    ALOGV("INFO(%s[%d]):(%d)(%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        index,
        shot_ext->node_group.capture[index].input.cropRegion[0],
        shot_ext->node_group.capture[index].input.cropRegion[1],
        shot_ext->node_group.capture[index].input.cropRegion[2],
        shot_ext->node_group.capture[index].input.cropRegion[3]);
}

void setMetaNodeCaptureOutputSize(struct camera2_shot_ext * shot_ext, int index, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
    shot_ext->node_group.capture[index].output.cropRegion[0] = x;
    shot_ext->node_group.capture[index].output.cropRegion[1] = y;
    shot_ext->node_group.capture[index].output.cropRegion[2] = w;
    shot_ext->node_group.capture[index].output.cropRegion[3] = h;

    ALOGV("INFO(%s[%d]):(%d)(%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        index,
        shot_ext->node_group.capture[index].output.cropRegion[0],
        shot_ext->node_group.capture[index].output.cropRegion[1],
        shot_ext->node_group.capture[index].output.cropRegion[2],
        shot_ext->node_group.capture[index].output.cropRegion[3]);
}

void setMetaBypassDrc(struct camera2_shot_ext *shot_ext, int value)
{
    shot_ext->drc_bypass = value;
}

void setMetaBypassDis(struct camera2_shot_ext *shot_ext, int value)
{
    shot_ext->dis_bypass = value;
}

void setMetaBypassDnr(struct camera2_shot_ext *shot_ext, int value)
{
    shot_ext->dnr_bypass = value;
}

void setMetaBypassFd(struct camera2_shot_ext *shot_ext, int value)
{
    shot_ext->fd_bypass = value;
}

void setMetaSetfile(struct camera2_shot_ext *shot_ext, int value)
{
    shot_ext->setfile = value;
}


int mergeSetfileYuvRange(int setfile, int yuvRange)
{
    int ret = setfile;

    ret &= (0x0000ffff);
    ret |= (yuvRange << 16);

    return ret;
}

int getPlaneSizeFlite(int width, int height)
{
    int PlaneSize;
    int Alligned_Width;
    int Bytes;

    Alligned_Width = (width + 9) / 10 * 10;
    Bytes = Alligned_Width * 8 / 5 ;

    PlaneSize = Bytes * height;

    return PlaneSize;
}

int getBayerLineSize(int width, int bayerFormat)
{
    int bytesPerLine = 0;

    if (width <= 0) {
        ALOGE("ERR(%s[%d]):Invalid input width size (%d)", __FUNCTION__, __LINE__, width);
        return bytesPerLine;
    }

    switch (bayerFormat) {
    case V4L2_PIX_FMT_SBGGR16:
        bytesPerLine = ROUND_UP(width * 2, CAMERA_16PX_ALIGN);
        break;
    case V4L2_PIX_FMT_SBGGR12:
        bytesPerLine = ROUND_UP((width * 3 / 2), CAMERA_16PX_ALIGN);
        break;
    case V4L2_PIX_FMT_SBGGR10:
        bytesPerLine = ROUND_UP((width * 5 / 4), CAMERA_16PX_ALIGN);
        break;
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_SGRBG8:
        bytesPerLine = ROUND_UP(width , CAMERA_16PX_ALIGN);
        break;
    default:
        ALOGW("WRN(%s[%d]):Invalid bayer format(%d)", __FUNCTION__, __LINE__, bayerFormat);
        bytesPerLine = ROUND_UP(width * 2, CAMERA_16PX_ALIGN);
        break;
    }

    return bytesPerLine;
}

int getBayerPlaneSize(int width, int height, int bayerFormat)
{
    int planeSize = 0;
    int bytesPerLine = 0;

    if (width <= 0 || height <= 0) {
        ALOGE("ERR(%s[%d]):Invalid input size (%d x %d)", __FUNCTION__, __LINE__, width, height);
        return planeSize;
    }

    bytesPerLine = getBayerLineSize(width, bayerFormat);
    planeSize = bytesPerLine * height;

    return planeSize;
}

bool directDumpToFile(ExynosCameraBuffer *buffer, uint32_t cameraId, uint32_t frameCount)
{
    time_t rawtime;
    struct tm* timeinfo;
    FILE *file = NULL;
    char fileName[EXYNOS_CAMERA_NAME_STR_SIZE];

    if (buffer == NULL) {
        ALOGE("Invalid ExynosBuffer");
        return false;
    }

    if (buffer->addr[0] == NULL) {
        ALOGE("Need to mmap the virtual address(%s)", buffer->bufMgrNm);
        return false;
    }

    if (buffer->status.position == EXYNOS_CAMERA_BUFFER_POSITION_IN_DRIVER) {
        ALOGE("Can't dump this buffer(%s, %d)", buffer->bufMgrNm, buffer->index);
        return false;
    }

    for (int batchIndex = 0; batchIndex < buffer->batchSize; batchIndex++) {
        /* skip meta plane */
        int startPlaneIndex = batchIndex * (buffer->planeCount - 1);
        int endPlaneIndex = (batchIndex + 1) * (buffer->planeCount - 1);

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        snprintf(fileName, sizeof(fileName), DEBUG_DUMP_NAME,
                cameraId, buffer->bufMgrNm, frameCount, buffer->index, batchIndex,
                timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday,
                timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

        file = fopen(fileName, "w");
        if (file == NULL) {
            ALOGE("ERR(%s):open(%s) fail", __func__, fileName);
            return false;
        }

        fflush(stdout);
        for (int plane = startPlaneIndex; plane < endPlaneIndex; plane++) {
            fwrite(buffer->addr[plane], 1, buffer->size[plane], file);
            ALOGI("filedump(%s, [%d]size(%d) s:%d, e:%d",
                    fileName, plane, buffer->size[plane],
                    startPlaneIndex, endPlaneIndex);
        }
        fflush(file);

        fclose(file);
        file = NULL;

        ALOGD("filedump(%s) is successed!!", fileName);
    }

    return true;
}

bool dumpToFile(char *filename, char *srcBuf, unsigned int size)
{
    FILE *yuvFd = NULL;
    char *buffer = NULL;

    yuvFd = fopen(filename, "w+");

    if (yuvFd == NULL) {
        ALOGE("ERR(%s):open(%s) fail",
            __func__, filename);
        return false;
    }

    buffer = (char *)malloc(size);

    if (buffer == NULL) {
        ALOGE("ERR(%s):malloc file", __func__);
        fclose(yuvFd);
        return false;
    }

    memcpy(buffer, srcBuf, size);

    fflush(stdout);

    fwrite(buffer, 1, size, yuvFd);

    fflush(yuvFd);

    if (yuvFd)
        fclose(yuvFd);
    if (buffer)
        free(buffer);

    ALOGD("DEBUG(%s):filedump(%s, size(%d) is successed!!",
            __func__, filename, size);

    return true;
}

bool dumpToFilePacked12(char *filename, char *srcBuf, unsigned int size)
{
    FILE *yuvFd = NULL;
    char *buffer = NULL;
    unsigned int unpackedSize = 0;

    yuvFd = fopen(filename, "w+");

    if (yuvFd == NULL) {
        ALOGE("ERR(%s):open(%s) fail",
                __func__, filename);
        return false;
    }

    unpackedSize = size * 4 / 3;
    buffer = (char *)malloc(unpackedSize);

    if (buffer == NULL) {
        ALOGE("ERR(%s):malloc file", __func__);
        fclose(yuvFd);
        return false;
    }

    char packedPixel[3];
    char unpackedPixel[4];
    char *dstAddr = buffer;
    memset(unpackedPixel, 0x00, sizeof(unpackedPixel));

    /* TODO: Must consider buffer stride */
    for (char *addr = srcBuf; addr < (srcBuf + size); addr += sizeof(packedPixel)) {
        memcpy(packedPixel, addr, sizeof(packedPixel));
        unpackedPixel[0] = packedPixel[0];
        unpackedPixel[1] = packedPixel[1] & 0x0F;
        unpackedPixel[2] = (packedPixel[1] & 0xF0) >> 4;
        unpackedPixel[2] |= ((packedPixel[2] & 0x0F) << 4);
        unpackedPixel[3] = (packedPixel[2] & 0xF0) >> 4;
        memcpy(dstAddr, unpackedPixel, sizeof(unpackedPixel));
        dstAddr += sizeof(unpackedPixel);
    }

    fflush(stdout);

    fwrite(buffer, 1, unpackedSize, yuvFd);

    fflush(yuvFd);

    if (yuvFd)
        fclose(yuvFd);
    if (buffer)
        free(buffer);

    ALOGD("DEBUG(%s):filedump(%s, size(%d) is successed!!",
            __func__, filename, size);

    return true;

}

bool dumpToFile2plane(char *filename, char *srcBuf, char *srcBuf1, unsigned int size, unsigned int size1)
{
    FILE *yuvFd = NULL;
    char *buffer = NULL;

    yuvFd = fopen(filename, "w+");

    if (yuvFd == NULL) {
        ALOGE("ERR(%s):open(%s) fail",
            __func__, filename);
        return false;
    }

    buffer = (char *)malloc(size + size1);

    if (buffer == NULL) {
        ALOGE("ERR(%s):malloc file", __func__);
        fclose(yuvFd);
        return false;
    }

    memcpy(buffer, srcBuf, size);
    memcpy(buffer + size, srcBuf1, size1);

    fflush(stdout);

    fwrite(buffer, 1, size + size1, yuvFd);

    fflush(yuvFd);

    if (yuvFd)
        fclose(yuvFd);
    if (buffer)
        free(buffer);

    ALOGD("DEBUG(%s):filedump(%s, size(%d) is successed!!",
            __func__, filename, size);

    return true;
}

status_t readFromFile(char *fileName, char *dstBuf, uint32_t size)
{
    status_t ret = NO_ERROR;
    FILE *fd = NULL;
    char *buffer = NULL;
    size_t readSize = 0;

    if (fileName == NULL || dstBuf == NULL || size == 0) {
        ALOGE("ERR(%s):Invalid parameters. fileName %p dstBuf %p size %d",
                __FUNCTION__, fileName, dstBuf, size);
        return BAD_VALUE;
    }

    fd = fopen(fileName, "rb");
    if (fd == NULL) {
        ALOGE("ERR(%s):Failed to open %s",
                __FUNCTION__, fileName);
        return INVALID_OPERATION;
    }

    buffer = (char *) malloc(size);
    if (buffer == NULL) {
        ALOGE("ERR(%s):Failed to alloc buffer. size %u",
                __FUNCTION__, size);
        fclose(fd);
        return INVALID_OPERATION;
    }

    readSize = fread(buffer, 1, size, fd);
    if (readSize != size) {
        ALOGW("WARN(%s):Image size mismatch! fileReadSize %zu bufferSize %u",
                __FUNCTION__, readSize, size);
        size = (size < readSize) ? size : readSize;
    }

    memcpy(dstBuf, buffer, size);

    ALOGD("DEBUG(%s):Success to read %s. size %u",
            __FUNCTION__, fileName, size);

    if (buffer != NULL) {
        free(buffer);
    }

    if (fd != NULL) {
        fclose(fd);
    }

    return ret;
}

status_t getYuvPlaneSize(int format, unsigned int *size,
                      unsigned int width, unsigned int height,
                      camera_pixel_size pixelSize)
{
    unsigned int frame_ratio = 1;
    unsigned int frame_size = 0;
    unsigned int src_bpp = 0;
    unsigned int src_planes = 0;

    if (getV4l2FormatInfo(format, &src_bpp, &src_planes, pixelSize) < 0){
        ALOGE("ERR(%s[%d]): invalid format(%x)", __FUNCTION__, __LINE__, format);
        return BAD_VALUE;
    }

    if (src_bpp == 0) {
        ALOGE("ERR(%s[%d]): invalid bpp(%d), format %c%c%c%c",
            __FUNCTION__, __LINE__, src_bpp,
            v4l2Format2Char(format, 0),
            v4l2Format2Char(format, 1),
            v4l2Format2Char(format, 2),
            v4l2Format2Char(format, 3));
        return BAD_VALUE;
    }

    src_planes = (src_planes == 0) ? 1 : src_planes;
    switch (pixelSize) {
    case CAMERA_PIXEL_SIZE_8BIT:
        frame_size = width * height;
        frame_ratio = 8 * (src_planes - 1) / (src_bpp - 8);
        break;
    case CAMERA_PIXEL_SIZE_10BIT:
        frame_size = (width * 2) * height;
        frame_ratio = 16 * (src_planes - 1) / (src_bpp - 16);
        break;
    case CAMERA_PIXEL_SIZE_PACKED_10BIT:
    case CAMERA_PIXEL_SIZE_8_2BIT:
        frame_size = ALIGN_UP((unsigned int)((double)(width * 2) * (10.0 / 16.0)), CAMERA_16PX_ALIGN) * height;
        frame_ratio = 10 * (src_planes - 1) / (src_bpp - 10);
        break;
    default:
        ALOGE("ERR(%s[%d]): invalid pixel size number(%d)", __FUNCTION__, __LINE__, pixelSize);
        return BAD_VALUE;
    }

    switch (src_planes) {
    case 1:
        switch (format) {
        case V4L2_PIX_FMT_BGR32:
        case V4L2_PIX_FMT_RGB32:
            size[0] = frame_size << 2;
            break;
        case V4L2_PIX_FMT_RGB565X:
        case V4L2_PIX_FMT_NV16:
        case V4L2_PIX_FMT_NV61:
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
        case V4L2_PIX_FMT_VYUY:
        case V4L2_PIX_FMT_YVYU:
            size[0] = frame_size << 1;
            break;
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_NV21M:
            size[0] = (frame_size * 3) >> 1;
            break;
        case V4L2_PIX_FMT_YVU420:
            size[0] = frame_size + (ALIGN((width >> 1), 16) * ((height >> 1) * 2));
            break;
        default:
            ALOGE("%s::invalid color type", __func__);
            return false;
            break;
        }
        size[1] = 0;
        size[2] = 0;
        break;
    case 2:
        switch (format) {
        case V4L2_PIX_FMT_NV12M_S10B:
            size[0] = NV12M_Y_SIZE(width, height) + NV12M_Y_2B_SIZE(width, height);
            size[1] = NV12M_CBCR_SIZE(width, height) + NV12M_CBCR_2B_SIZE(width, height);
            size[2] = 0;
            break;
        case V4L2_PIX_FMT_NV16M_S10B:
            size[0] = NV16M_Y_SIZE(width, height) + NV16M_Y_2B_SIZE(width, height);
            size[1] = NV16M_CBCR_SIZE(width, height) + NV16M_CBCR_2B_SIZE(width, height);
            size[2] = 0;
            break;
        default:
            size[0] = frame_size;
            size[1] = frame_size / frame_ratio;
            size[2] = 0;
            break;
        }
        break;
    case 3:
        size[0] = frame_size;
        size[1] = frame_size / frame_ratio;
        size[2] = frame_size / frame_ratio;
        break;
    default:
        ALOGE("%s::invalid color foarmt", __func__);
        return false;
        break;
    }

    return NO_ERROR;
}

status_t getV4l2FormatInfo(unsigned int v4l2_pixel_format,
                          unsigned int *bpp, unsigned int *planes, camera_pixel_size pixelSize)
{
    switch (v4l2_pixel_format) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
        *bpp    = 12;
        *planes = 1;
        break;
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_NV21M:
    case V4L2_PIX_FMT_NV12MT:
    case V4L2_PIX_FMT_NV12MT_16X16:
        *bpp    = 12;
        *planes = 2;
        break;
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YVU420:
        *bpp    = 12;
        *planes = 1;
        break;
    case V4L2_PIX_FMT_YUV420M:
    case V4L2_PIX_FMT_YVU420M:
        *bpp    = 12;
        *planes = 3;
        break;
    case V4L2_PIX_FMT_NV12M_P010:
        *planes = 2;
        if (pixelSize == CAMERA_PIXEL_SIZE_10BIT) {
            *bpp    = 24;
        } else if (pixelSize == CAMERA_PIXEL_SIZE_PACKED_10BIT) {
            *bpp    = 15;
        } else {
            *bpp    = 0;
        }
        break;
    case V4L2_PIX_FMT_NV12M_S10B:
        *bpp    = 15;
        *planes = 2;
        break;
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_YVYU:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_VYUY:
        *planes = 1;
        if (pixelSize == CAMERA_PIXEL_SIZE_8BIT) {
            *bpp    = 16;
        } else if (pixelSize == CAMERA_PIXEL_SIZE_10BIT) { /* Y210 format */
            *bpp    = 32;
        } else if (pixelSize == CAMERA_PIXEL_SIZE_PACKED_10BIT) { /* Y210 packed format */
            *bpp    = 20;
        } else {
            *bpp    = 0;
        }
        break;
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_NV61:
        *bpp    = 16;
        *planes = 1;
        break;
    case V4L2_PIX_FMT_NV16M:
    case V4L2_PIX_FMT_NV61M:
        *bpp    = 16;
        *planes = 2;
        break;
    case V4L2_PIX_FMT_NV16M_P210:
        *planes = 2;
        if (pixelSize == CAMERA_PIXEL_SIZE_10BIT) {
            *bpp    = 32;
        } else if (pixelSize == CAMERA_PIXEL_SIZE_PACKED_10BIT) {
            *bpp    = 20;
        } else {
            *bpp    = 0;
        }
        break;
    case V4L2_PIX_FMT_NV16M_S10B:
        *bpp    = 20;
        *planes = 2;
        break;
    case V4L2_PIX_FMT_YUV422P:
        *bpp    = 16;
        *planes = 3;
        break;
    case V4L2_PIX_FMT_SBGGR16:
        *bpp    = 16;
        *planes = 1;
        break;
    case V4L2_PIX_FMT_SBGGR12:
        *bpp    = 12;
        *planes = 1;
        break;
    case V4L2_PIX_FMT_SBGGR10:
        *bpp    = 10;
        *planes = 1;
        break;
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_SGRBG8:
        *bpp    = 8;
        *planes = 1;
        break;
    case V4L2_PIX_FMT_Z16:
        *bpp    = 16;
        *planes = 1;
        break;
    default:
        return BAD_VALUE;
        break;
    }

    return NO_ERROR;
}

int getYuvPlaneCount(unsigned int v4l2_pixel_format, camera_pixel_size pixelSize)
{
    int ret = 0;
    unsigned int bpp = 0;
    unsigned int planeCnt = 0;

    ret = getV4l2FormatInfo(v4l2_pixel_format, &bpp, &planeCnt, pixelSize);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]): BAD_VALUE", __FUNCTION__, __LINE__);
        return -1;
    }

    return planeCnt;
}

int displayExynosBuffer( ExynosCameraBuffer *buffer) {
        ALOGD("-----------------------------------------------");
        ALOGD(" buffer.index = %d ", buffer->index);
        ALOGD(" buffer.planeCount = %d ", buffer->planeCount);
        for(int i = 0 ; i < buffer->planeCount ; i++ ) {
            ALOGD(" buffer.fd[%d] = %d ", i, buffer->fd[i]);
            ALOGD(" buffer.size[%d] = %d ", i, buffer->size[i]);
            ALOGD(" buffer.addr[%d] = %p ", i, buffer->addr[i]);
        }
        ALOGD("-----------------------------------------------");
        return 0;
}

#ifdef SENSOR_NAME_GET_FROM_FILE
int getSensorIdFromFile(int camId)
{
    FILE *fp = NULL;
    int numread = -1;
    char sensor_name[50];
    int sensorName = -1;
    bool ret = true;

    if (camId == CAMERA_ID_BACK) {
        fp = fopen(SENSOR_NAME_PATH_BACK, "r");
        if (fp == NULL) {
            ALOGE("ERR(%s[%d]):failed to open sysfs entry", __FUNCTION__, __LINE__);
            goto err;
        }
#if defined(SENSOR_NAME_PATH_SECURE)
    } else if (camId == CAMERA_ID_SECURE) {
        fp = fopen(SENSOR_NAME_PATH_SECURE, "r");
        if (fp == NULL) {
            ALOGE("ERR(%s[%d]):failed to open sysfs entry", __FUNCTION__, __LINE__);
            goto err;
        }
#endif
#ifdef USE_DUAL_CAMERA
    } else if (camId == CAMERA_ID_BACK_1) {
        fp = fopen(SENSOR_NAME_PATH_BACK_1, "r");
        if (fp == NULL) {
            ALOGE("ERR(%s[%d]):failed to open sysfs entry", __FUNCTION__, __LINE__);
            goto err;
        }
#endif
    } else {
        fp = fopen(SENSOR_NAME_PATH_FRONT, "r");
        if (fp == NULL) {
            ALOGE("ERR(%s[%d]):failed to open sysfs entry", __FUNCTION__, __LINE__);
            goto err;
        }
    }

    if (fgets(sensor_name, sizeof(sensor_name), fp) == NULL) {
        ALOGE("ERR(%s[%d]):failed to read sysfs entry", __FUNCTION__, __LINE__);
	    goto err;
    }

    numread = strlen(sensor_name);
    ALOGD("DEBUG(%s[%d]):Sensor name is %s(%d)", __FUNCTION__, __LINE__, sensor_name, numread);

    /* TODO: strncmp for check sensor name, str is vendor specific sensor name
     * ex)
     *    if (strncmp((const char*)sensor_name, "str", numread - 1) == 0) {
     *        sensorName = SENSOR_NAME_IMX135;
     *    }
     */
    sensorName = atoi(sensor_name);

err:
    if (fp != NULL)
        fclose(fp);

    return sensorName;
}
#endif

#ifdef SENSOR_FW_GET_FROM_FILE
const char *getSensorFWFromFile(struct ExynosCameraSensorInfoBase *info, int camId)
{
    FILE *fp = NULL;
    int numread = -1;

    if (camId == CAMERA_ID_BACK) {
        fp = fopen(SENSOR_FW_PATH_BACK, "r");
        if (fp == NULL) {
            ALOGE("ERR(%s[%d]):failed to open sysfs entry", __FUNCTION__, __LINE__);
            goto err;
        }
    }
#ifdef USE_DUAL_CAMERA
    else if (camId == CAMERA_ID_BACK_1) {
        fp = fopen(SENSOR_FW_PATH_BACK_1, "r");
        if (fp == NULL) {
            ALOGE("ERR(%s[%d]):failed to open sysfs entry", __FUNCTION__, __LINE__);
            goto err;
        }
    }
#endif
    else {
        fp = fopen(SENSOR_FW_PATH_FRONT, "r");
        if (fp == NULL) {
            ALOGE("ERR(%s[%d]):failed to open sysfs entry", __FUNCTION__, __LINE__);
            goto err;
        }
    }
    if (fgets(info->sensor_fw, sizeof(info->sensor_fw), fp) == NULL) {
        ALOGE("ERR(%s[%d]):failed to read sysfs entry", __FUNCTION__, __LINE__);
	    goto err;
    }

    numread = strlen(info->sensor_fw);
    ALOGD("DEBUG(%s[%d]):Sensor fw(len:%d) is %s", __FUNCTION__, __LINE__, numread, info->sensor_fw);

err:
    if (fp != NULL)
        fclose(fp);

    return (const char *)info->sensor_fw;
}
#endif


int checkBit(unsigned int *target, int index)
{
    int ret = 0;
    if (*target & (1 << index)) {
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

void clearBit(unsigned int *target, int index, bool isStatePrint)
{
    *target = *target &~ (1 << index);

    if (isStatePrint)
        ALOGD("INFO(%s[%d]):(0x%x)", __FUNCTION__, __LINE__, *target);
}

void setBit(unsigned int *target, int index, bool isStatePrint)
{
    *target = *target | (1 << index);

    if (isStatePrint)
        ALOGD("INFO(%s[%d]):(0x%x)", __FUNCTION__, __LINE__, *target);
}

void resetBit(unsigned int *target, int value, bool isStatePrint)
{
    *target = value;

    if (isStatePrint)
        ALOGD("INFO(%s[%d]):(0x%x)", __FUNCTION__, __LINE__, *target);
}

status_t addBayerBuffer(struct ExynosCameraBuffer *srcBuf,
                        struct ExynosCameraBuffer *dstBuf,
                        __unused ExynosRect *dstRect,
#ifndef ADD_BAYER_BY_NEON
                        __unused
#endif
                        bool isPacked)
{
    status_t ret = NO_ERROR;

    if (srcBuf == NULL) {
        ALOGE("ERR(%s[%d]):srcBuf == NULL",  __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    /* assume bayer buffer is 0 */
    if (srcBuf->size[0] != dstBuf->size[0])
        ALOGW("WARN(%s[%d]):srcBuf->size[0] (%d)!= dstBuf->size[0](%d). weird",
                __FUNCTION__, __LINE__, srcBuf->size[0], dstBuf->size[0]);

    unsigned int copySize = (srcBuf->size[0] < dstBuf->size[0]) ? srcBuf->size[0] : dstBuf->size[0];

#ifdef ADD_BAYER_BY_NEON
    if (isPacked == true)
        ret = addBayerBufferByNeonPacked(srcBuf, dstBuf, dstRect, copySize);
    else
        ret = addBayerBufferByNeon(srcBuf, dstBuf, copySize);
#else
    ret = addBayerBufferByCpu(srcBuf, dstBuf, copySize);
#endif

    if (ret != NO_ERROR)
        ALOGE("ERR(%s[%d]):addBayerBuffer() fail", __FUNCTION__, __LINE__);

    return ret;
}

status_t addBayerBufferByNeon(struct ExynosCameraBuffer *srcBuf,
                              struct ExynosCameraBuffer *dstBuf,
                              unsigned int copySize)
{
    if (srcBuf->addr[0] == NULL) {
        ALOGE("ERR(%s[%d]):srcBuf->addr[0] == NULL",  __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (dstBuf->addr[0] == NULL) {
        ALOGE("ERR(%s[%d]):dstBuf->addr[0] == NULL",  __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    /* bayer is max 16bit, so add by short */
    unsigned short*firstSrcAddr = (unsigned short *)srcBuf->addr[0];
    unsigned short*firstDstAddr = (unsigned short *)dstBuf->addr[0];
    unsigned short*srcAddr = firstSrcAddr;
    unsigned short*dstAddr = firstDstAddr;

    /*
     * loop as copySize / 32 byte
     * 32 byte is perfect align size of cache.
     * 64 byte is not faster than 32byte.
     */
    unsigned int alignByte = 64;
    unsigned int alignShort = 32;
    unsigned int realCopySize = copySize / alignByte;
    unsigned int remainedCopySize = copySize % alignByte;

    ALOGD("DEBUG(%s[%d]):srcAddr(%p), dstAddr(%p), copySize(%d), sizeof(short)(%zu),\
            alignByte(%d), realCopySize(%d), remainedCopySize(%d)",
            __FUNCTION__, __LINE__, srcAddr, dstAddr, copySize, sizeof(short), alignByte,
            realCopySize, remainedCopySize);

    unsigned short* src0_ptr, *src1_ptr;
    uint16x8_t src0_u16x8_0;
    uint16x8_t src0_u16x8_1;
    uint16x8_t src0_u16x8_2;
    uint16x8_t src0_u16x8_3;

    src0_ptr = dstAddr;
    src1_ptr = srcAddr;

    for (unsigned int i = 0; i < realCopySize; i++) {
        src0_u16x8_0 = vqaddq_u16(vshlq_n_u16(vld1q_u16((uint16_t*)(src0_ptr)), 6),
                vshlq_n_u16(vld1q_u16((uint16_t*)(src1_ptr)), 6));
        src0_u16x8_1 = vqaddq_u16(vshlq_n_u16(vld1q_u16((uint16_t*)(src0_ptr + 8)), 6),
                vshlq_n_u16(vld1q_u16((uint16_t*)(src1_ptr + 8)), 6));
        src0_u16x8_2 = vqaddq_u16(vshlq_n_u16(vld1q_u16((uint16_t*)(src0_ptr + 16)), 6),
                vshlq_n_u16(vld1q_u16((uint16_t*)(src1_ptr + 16)), 6));
        src0_u16x8_3 = vqaddq_u16(vshlq_n_u16(vld1q_u16((uint16_t*)(src0_ptr + 24)), 6),
                vshlq_n_u16(vld1q_u16((uint16_t*)(src1_ptr + 24)), 6));

        vst1q_u16((src0_ptr), vshrq_n_u16(src0_u16x8_0, 6));
        vst1q_u16((src0_ptr + 8), vshrq_n_u16(src0_u16x8_1, 6));
        vst1q_u16((src0_ptr + 16),vshrq_n_u16(src0_u16x8_2, 6));
        vst1q_u16((src0_ptr + 24),vshrq_n_u16(src0_u16x8_3, 6));

        src0_ptr = firstDstAddr + (alignShort * (i + 1));
        src1_ptr = firstSrcAddr + (alignShort * (i + 1));
    }

    for (unsigned int i = 0; i < remainedCopySize; i++) {
        dstAddr[i] = SATURATING_ADD(dstAddr[i], srcAddr[i]);
    }

    return NO_ERROR;
}

status_t addBayerBufferByNeonPacked(struct ExynosCameraBuffer *srcBuf,
                                    struct ExynosCameraBuffer *dstBuf,
                                    ExynosRect *dstRect,
                                    unsigned int copySize)
{
    if (srcBuf->addr[0] == NULL) {
        ALOGE("ERR(%s[%d]):srcBuf->addr[0] == NULL",  __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (dstBuf->addr[0] == NULL) {
        ALOGE("ERR(%s[%d]):dstBuf->addr[0] == NULL",  __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    /* bayer is max 16bit, so add by short */
    unsigned char *firstSrcAddr = (unsigned char *)srcBuf->addr[0];
    unsigned char *firstDstAddr = (unsigned char *)dstBuf->addr[0];
    unsigned char *srcAddr = firstSrcAddr;
    unsigned char *dstAddr = firstDstAddr;

    /*
     *      * loop as copySize / 32 byte
     *      * 32 byte is perfect align size of cache.
     *      * 64 byte is not faster than 32byte.
     *      */

    unsigned int alignByte = 12;
    unsigned int realCopySize = copySize / alignByte;
    unsigned int remainedCopySize = copySize % alignByte;

    uint16x8_t src_u16x8_0 = {0, 0, 0, 0, 0, 0, 0, 0};
    uint16x8_t dst_u16x8_0 = {0, 0, 0, 0, 0, 0, 0, 0};

    unsigned int width_byte = dstRect->w * 12 / 8;
    width_byte = ALIGN_UP(width_byte, 16);

    ALOGD("DEBUG(%s[%d]):srcAddr(%p), dstAddr(%p), copySize(%d), sizeof(short)(%d),\
        alignByte(%d), realCopySize(%d), remainedCopySize(%d), pixel width(%d), pixel height(%d),\
        16 aligned byte width(%d)",
        __FUNCTION__, __LINE__, srcAddr, dstAddr, copySize, (int)sizeof(short),
        alignByte, realCopySize, remainedCopySize, dstRect->w, dstRect->h, width_byte);

    unsigned short dstPix_0, dstPix_1, dstPix_2, dstPix_3, dstPix_4, dstPix_5, dstPix_6, dstPix_7;
    unsigned short srcPix_0, srcPix_1, srcPix_2, srcPix_3, srcPix_4, srcPix_5, srcPix_6, srcPix_7;
    unsigned int col;

    for (unsigned int row = 0; row < (unsigned int)dstRect->h; row++) {
        for (col = 0; col + alignByte <= width_byte; col += alignByte) {
            dstAddr = firstDstAddr + width_byte * row + col;
            srcAddr = firstSrcAddr + width_byte * row + col;

            unsigned short temp_0 = dstAddr[0];
            unsigned short temp_1 = dstAddr[1];
            unsigned short temp_cmbd = COMBINE_P0(temp_0, temp_1);
            unsigned short temp_2 = dstAddr[2];
            unsigned short temp_cmbd2 = COMBINE_P1(temp_1, temp_2);

            dstPix_0 = temp_cmbd - 32;
            dstPix_1 = temp_cmbd2 - 32;

            temp_0 = dstAddr[3];
            temp_1 = dstAddr[4];
            temp_cmbd = COMBINE_P0(temp_0, temp_1);
            temp_2 = dstAddr[5];
            temp_cmbd2 = COMBINE_P1(temp_1, temp_2);

            dstPix_2 = temp_cmbd - 32;
            dstPix_3 = temp_cmbd2 - 32;

            temp_0 = dstAddr[6];
            temp_1 = dstAddr[7];
            temp_cmbd = COMBINE_P0(temp_0, temp_1);
            temp_2 = dstAddr[8];
            temp_cmbd2 = COMBINE_P1(temp_1, temp_2);

            dstPix_4 = temp_cmbd - 32;
            dstPix_5 = temp_cmbd2 - 32;

            temp_0 = dstAddr[9];
            temp_1 = dstAddr[10];
            temp_cmbd = COMBINE_P0(temp_0, temp_1);
            temp_2 = dstAddr[11];
            temp_cmbd2 = COMBINE_P1(temp_1, temp_2);

            dstPix_6 = temp_cmbd - 32;
            dstPix_7 = temp_cmbd2 - 32;

            temp_0 = srcAddr[0];
            temp_1 = srcAddr[1];
            temp_cmbd = COMBINE_P0(temp_0, temp_1);
            temp_2 = srcAddr[2];
            temp_cmbd2 = COMBINE_P1(temp_1, temp_2);

            srcPix_0 = temp_cmbd - 32;
            srcPix_1 = temp_cmbd2 - 32;

            temp_0 = srcAddr[3];
            temp_1 = srcAddr[4];
            temp_cmbd = COMBINE_P0(temp_0, temp_1);
            temp_2 = srcAddr[5];
            temp_cmbd2 = COMBINE_P1(temp_1, temp_2);

            srcPix_2 = temp_cmbd - 32;
            srcPix_3 = temp_cmbd2 - 32;

            temp_0 = srcAddr[6];
            temp_1 = srcAddr[7];
            temp_cmbd = COMBINE_P0(temp_0, temp_1);
            temp_2 = srcAddr[8];
            temp_cmbd2 = COMBINE_P1(temp_1, temp_2);
            srcPix_4 = temp_cmbd - 32;
            srcPix_5 = temp_cmbd2 - 32;

            temp_0 = srcAddr[9];
            temp_1 = srcAddr[10];
            temp_cmbd = COMBINE_P0(temp_0, temp_1);
            temp_2 = srcAddr[11];
            temp_cmbd2 = COMBINE_P1(temp_1, temp_2);
            srcPix_6 = temp_cmbd - 32;
            srcPix_7 = temp_cmbd2 - 32;

            src_u16x8_0 =  vsetq_lane_u16(srcPix_0, src_u16x8_0, 0);
            src_u16x8_0 =  vsetq_lane_u16(srcPix_1, src_u16x8_0, 1);
            src_u16x8_0 =  vsetq_lane_u16(srcPix_2, src_u16x8_0, 2);
            src_u16x8_0 =  vsetq_lane_u16(srcPix_3, src_u16x8_0, 3);
            src_u16x8_0 =  vsetq_lane_u16(srcPix_4, src_u16x8_0, 4);
            src_u16x8_0 =  vsetq_lane_u16(srcPix_5, src_u16x8_0, 5);
            src_u16x8_0 =  vsetq_lane_u16(srcPix_6, src_u16x8_0, 6);
            src_u16x8_0 =  vsetq_lane_u16(srcPix_7, src_u16x8_0, 7);

            dst_u16x8_0 =  vsetq_lane_u16(dstPix_0, dst_u16x8_0, 0);
            dst_u16x8_0 =  vsetq_lane_u16(dstPix_1, dst_u16x8_0, 1);
            dst_u16x8_0 =  vsetq_lane_u16(dstPix_2, dst_u16x8_0, 2);
            dst_u16x8_0 =  vsetq_lane_u16(dstPix_3, dst_u16x8_0, 3);
            dst_u16x8_0 =  vsetq_lane_u16(dstPix_4, dst_u16x8_0, 4);
            dst_u16x8_0 =  vsetq_lane_u16(dstPix_5, dst_u16x8_0, 5);
            dst_u16x8_0 =  vsetq_lane_u16(dstPix_6, dst_u16x8_0, 6);
            dst_u16x8_0 =  vsetq_lane_u16(dstPix_7, dst_u16x8_0, 7);

            dst_u16x8_0 = vqaddq_u16(vshlq_n_u16(dst_u16x8_0, 6), vshlq_n_u16(src_u16x8_0, 6));
            dst_u16x8_0 = vshrq_n_u16(dst_u16x8_0, 6);

            dstPix_0 = vgetq_lane_u16(dst_u16x8_0, 0);
            dstPix_1 = vgetq_lane_u16(dst_u16x8_0, 1);
            dstPix_2 = vgetq_lane_u16(dst_u16x8_0, 2);
            dstPix_3 = vgetq_lane_u16(dst_u16x8_0, 3);
            dstPix_4 = vgetq_lane_u16(dst_u16x8_0, 4);
            dstPix_5 = vgetq_lane_u16(dst_u16x8_0, 5);
            dstPix_6 = vgetq_lane_u16(dst_u16x8_0, 6);
            dstPix_7 = vgetq_lane_u16(dst_u16x8_0, 7);

            dstAddr[0] = (unsigned char)(dstPix_0);
            dstAddr[1] = (unsigned char)((COMBINE_P3(dstPix_0, dstPix_1)));
            dstAddr[2] = (unsigned char)(dstPix_1 >> 4);
            dstAddr[3] = (unsigned char)(dstPix_2);
            dstAddr[4] = (unsigned char)((COMBINE_P3(dstPix_2, dstPix_3)));
            dstAddr[5] = (unsigned char)(dstPix_3 >> 4);
            dstAddr[6] = (unsigned char)(dstPix_4);
            dstAddr[7] = (unsigned char)((COMBINE_P3(dstPix_4, dstPix_5)));
            dstAddr[8] = (unsigned char)(dstPix_5 >> 4);
            dstAddr[9] = (unsigned char)(dstPix_6);
            dstAddr[10] = (unsigned char)((COMBINE_P3(dstPix_6, dstPix_7)));
            dstAddr[11] = (unsigned char)(dstPix_7 >> 4);
        }
    }

#if 0
    /* for the case of pixel width which is not a multiple of 8. The section of codes need to be verified */
    for (unsigned int i = 0; i < remainedCopySize; i += 3) {
        unsigned char temp_0 = dstAddr[0];
        unsigned char temp_1 = dstAddr[1];
        unsigned char temp_cmbd = COMBINE_P0(temp_0, temp_1);
        unsigned char temp_2 = dstAddr[2];
        unsigned char temp_cmbd2 = COMBINE_P1(temp_1, temp_2);
        unsigned char dstPix_0 = temp_cmbd;
        unsigned char dstPix_1 = temp_cmbd2;

        temp_0 = srcAddr[0];
        temp_1 = srcAddr[1];
        temp_cmbd = COMBINE_P0(temp_0, temp_1);
        temp_2 = srcAddr[2];
        temp_cmbd2 = COMBINE_P1(temp_1, temp_2);
        srcPix_0 = temp_cmbd;
        srcPix_1 = temp_cmbd2;

        dstPix_0 = SATURATING_ADD(dstPix_0, srcPix_0);
        dstPix_1 = SATURATING_ADD(dstPix_1, srcPix_1);

        dstAddr[0] = (unsigned char)(dstPix_0);
        dstAddr[1] = (unsigned char)((COMBINE_P3(dstPix_0, dstPix_1)));
        dstAddr[2] = (unsigned char)(dstPix_1 >> 4);

        dstAddr += 3;
        srcAddr += 3;
    }
#endif

    return NO_ERROR;
}

status_t addBayerBufferByCpu(struct ExynosCameraBuffer *srcBuf,
                             struct ExynosCameraBuffer *dstBuf,
                             unsigned int copySize)
{
    if (srcBuf->addr[0] == NULL) {
        ALOGE("ERR(%s[%d]):srcBuf->addr[0] == NULL",  __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (dstBuf->addr[0] == NULL) {
        ALOGE("ERR(%s[%d]):dstBuf->addr[0] == NULL",  __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    /* bayer is max 16bit, so add by short */
    unsigned short *firstSrcAddr = (unsigned short *)srcBuf->addr[0];
    unsigned short *firstDstAddr = (unsigned short *)dstBuf->addr[0];
    unsigned short *srcAddr = firstSrcAddr;
    unsigned short *dstAddr = firstDstAddr;

    /*
     * loop as copySize / 32 byte
     * 32 byte is perfect align size of cache.
     * 64 byte is not faster than 32byte.
     */
    unsigned int alignByte = 32;
    unsigned int alignShort = 16;
    unsigned int realCopySize     = copySize / alignByte;
    unsigned int remainedCopySize = copySize % alignByte;

    ALOGD("DEBUG(%s[%d]):srcAddr(%p), dstAddr(%p), copySize(%d), sizeof(short)(%zu),\
            alignByte(%d), realCopySize(%d), remainedCopySize(%d)",
            __FUNCTION__, __LINE__, srcAddr, dstAddr, copySize, sizeof(short), alignByte,
            realCopySize, remainedCopySize);

    for (unsigned int i = 0; i < realCopySize; i++) {
        dstAddr[0] = SATURATING_ADD(dstAddr[0], srcAddr[0]);
        dstAddr[1] = SATURATING_ADD(dstAddr[1], srcAddr[1]);
        dstAddr[2] = SATURATING_ADD(dstAddr[2], srcAddr[2]);
        dstAddr[3] = SATURATING_ADD(dstAddr[3], srcAddr[3]);
        dstAddr[4] = SATURATING_ADD(dstAddr[4], srcAddr[4]);
        dstAddr[5] = SATURATING_ADD(dstAddr[5], srcAddr[5]);
        dstAddr[6] = SATURATING_ADD(dstAddr[6], srcAddr[6]);
        dstAddr[7] = SATURATING_ADD(dstAddr[7], srcAddr[7]);
        dstAddr[8] = SATURATING_ADD(dstAddr[8], srcAddr[8]);
        dstAddr[9] = SATURATING_ADD(dstAddr[9], srcAddr[9]);
        dstAddr[10] = SATURATING_ADD(dstAddr[10], srcAddr[10]);
        dstAddr[11] = SATURATING_ADD(dstAddr[11], srcAddr[11]);
        dstAddr[12] = SATURATING_ADD(dstAddr[12], srcAddr[12]);
        dstAddr[13] = SATURATING_ADD(dstAddr[13], srcAddr[13]);
        dstAddr[14] = SATURATING_ADD(dstAddr[14], srcAddr[14]);
        dstAddr[15] = SATURATING_ADD(dstAddr[15], srcAddr[15]);

        // jump next 32bytes.
        //srcAddr += alignShort;
        //dstAddr += alignShort;
        /* This is faster on compiler lever */
        srcAddr = firstSrcAddr + (alignShort * (i + 1));
        dstAddr = firstDstAddr + (alignShort * (i + 1));
    }

    for (unsigned int i = 0; i < remainedCopySize; i++) {
        dstAddr[i] = SATURATING_ADD(dstAddr[i], srcAddr[i]);
    }

    return NO_ERROR;
}

char clip(int i)
{
    if(i < 0)
        return 0;
    else if(i > 255)
        return 255;
    else
        return i;
}

/*
 **    The only convertingYUYVtoRGB888() code is covered by BSD.
 **    URL from which the open source has been downloaded is
 **    http://www.mathworks.com/matlabcentral/fileexchange/26249-yuy2-to-rgb-converter/content/YUY2toRGB.zip
 */
void convertingYUYVtoRGB888(char *dstBuf, char *srcBuf, int width, int height)
{
    int Y0, Y1, U, V, C0, C1, D, E;

    for(int y = 0; y < height; y++) {
        for(int x = 0; x < (width / 2); x++)
        {
            Y0 = srcBuf[(2 * y * width) + (4 * x)];
            Y1 = srcBuf[(2 * y * width) + (4 * x) + 2];
            U = srcBuf[(2 * y * width) + (4 * x) + 1];
            V = srcBuf[(2 * y * width) + (4 * x) + 3];
            C0 = Y0 - 16;
            C1 = Y1 - 16;
            D = U - 128;
            E = V - 128;
            dstBuf[6 * (x + (y * width / 2))] =
                clip(((298 * C0) + (409 * E) + 128) >> 8);   // R0
            dstBuf[6 * (x + (y * width / 2)) + 1] =
                clip(((298 * C0) - (100 * D) - (208 * E) + 128) >> 8); // G0
            dstBuf[6 * (x + (y * width / 2)) + 2] =
                clip(((298 * C0) + (516 * D) + 128) >> 8);   // B0
            dstBuf[6 * (x + (y * width / 2)) + 3] =
                clip(((298 * C1) + (409 * E) + 128) >> 8);   // R1
            dstBuf[6 * (x + (y * width / 2)) + 4] =
                clip(((298 * C1) - (100 * D) - (208 * E) + 128) >> 8); // G1
            dstBuf[6 * (x + (y * width / 2)) + 5] =
                clip(((298 * C1) + (516 * D) + 128) >> 8);   // B1
        }
    }
}

int getFliteNodenum(int cameraId)
{
    int fliteNodeNum = FIMC_IS_VIDEO_SS0_NUM;

    switch (cameraId) {
    case CAMERA_ID_BACK:
        fliteNodeNum = MAIN_CAMERA_FLITE_NUM;
        break;
    case CAMERA_ID_FRONT:
        fliteNodeNum = FRONT_CAMERA_FLITE_NUM;
        break;
#ifdef USE_DUAL_CAMERA
    case CAMERA_ID_BACK_1:
        fliteNodeNum = MAIN_1_CAMERA_FLITE_NUM;
        break;
    case CAMERA_ID_FRONT_1:
        fliteNodeNum = FRONT_1_CAMERA_FLITE_NUM;
        break;
#endif
    case CAMERA_ID_SECURE:
        /*HACK : SECURE_CAMERA_FLITE_NUM is not defined, so use FIMC_IS_VIDEO_SS3_NUM */
        fliteNodeNum = FIMC_IS_VIDEO_SS3_NUM;
        break;
    default:
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Unexpected cameraId(%d), assert!!!!",
            __FUNCTION__, __LINE__, cameraId);
        break;
    }

    return fliteNodeNum;
}

int getFliteCaptureNodenum(int cameraId, int fliteOutputNode)
{
    if (fliteOutputNode <= 0) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):cameraId(%d):Invalid fliteOutputNode(%d). assert!!!!",
            __FUNCTION__, __LINE__, cameraId, fliteOutputNode);
    }

    int ouputIndex = 0;
    int fliteCaptureNodeNum = 0;

    ouputIndex = fliteOutputNode - FIMC_IS_VIDEO_BAS_NUM;
    if (ouputIndex <= 0) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):cameraId(%d):Invalid ouputIndex(%d). assert!!!!",
            __FUNCTION__, __LINE__, cameraId, ouputIndex);
    }

    /*
     * in case of fliteOutputNode is FIMC_IS_VIDEO_SS2_NUM(103))
     * (FIMC_IS_VIDEO_SS1VC0_NUM(214) - FIMC_IS_VIDEO_SS0VC0_NUM(210)) * (3 - 1) = 4 * 2 = 8
     * 8 + FIMC_IS_VIDEO_SS0VC0_NUM(210) = FIMC_IS_VIDEO_SS2VC0_NUM(218)
     */
    fliteCaptureNodeNum = (FIMC_IS_VIDEO_SS1VC0_NUM - FIMC_IS_VIDEO_SS0VC0_NUM) * (ouputIndex - 1);
    fliteCaptureNodeNum += FIMC_IS_VIDEO_SS0VC0_NUM;

    if (FIMC_IS_VIDEO_MAX_NUM <= fliteCaptureNodeNum) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):cameraId(%d):Invalid fliteCaptureNodeNum(%d) by fliteOutputNode(%d), ouputIndex(%d). assert!!!!",
            __FUNCTION__, __LINE__, cameraId, fliteCaptureNodeNum, fliteOutputNode, ouputIndex);
    }

    return fliteCaptureNodeNum;
}

#ifdef SUPPORT_DEPTH_MAP
int getDepthVcNodeNum(int cameraId)
{
    int depthVcNodeNum = FIMC_IS_VIDEO_SS0VC1_NUM;

    depthVcNodeNum = (cameraId == CAMERA_ID_BACK) ? MAIN_CAMERA_DEPTH_VC_NUM : FRONT_CAMERA_DEPTH_VC_NUM;

    return depthVcNodeNum;
}
#endif // SUPPORT_DEPTH_MAP

void checkAndroidVersion(void) {
    char value[PROPERTY_VALUE_MAX] = {0};
    char targetAndroidVersion[PROPERTY_VALUE_MAX] = {0};

    snprintf(targetAndroidVersion, PROPERTY_VALUE_MAX, "%d.%d", TARGET_ANDROID_VER_MAJ, TARGET_ANDROID_VER_MIN);

    property_get("ro.build.version.release", value, "0");

    if (strncmp(targetAndroidVersion, value, PROPERTY_VALUE_MAX))
        ALOGD("DEBUG(%s[%d]): Tartget Android version (%s), build version (%s)",
                __FUNCTION__, __LINE__, targetAndroidVersion, value);
    else
        ALOGI("Andorid build version release %s", value);
}

int calibratePosition(int w, int new_w, int pos)
{
    return (float)(pos * new_w) / (float)w;
}

status_t updateYsumBuffer(struct ysum_data *ysumdata, ExynosCameraBuffer *dstBuf)
{
    ExynosVideoMeta *videoMeta = NULL;

    if (ysumdata == NULL) {
        ALOGE("ysumdata null");
        return -BAD_VALUE;
    }

    if (dstBuf == NULL) {
        ALOGE(" dstBuf null");
        return -BAD_VALUE;
    }

    if (dstBuf->batchSize > 1) {
        ALOGD("YSUM recording does not support batch mode. only works at 30, 60 fps.");
        return NO_ERROR;
    }

    int videoMetaPlaneIndex = dstBuf->getMetaPlaneIndex() - 1;
    if (dstBuf->size[videoMetaPlaneIndex] != EXYNOS_CAMERA_VIDEO_META_PLANE_SIZE) {
        android_printAssert(NULL, LOG_TAG,
                        "ASSERT(%s):Invalid access to video met plane. planeIndex %d size %d",
                        __FUNCTION__, videoMetaPlaneIndex, dstBuf->size[videoMetaPlaneIndex]);
        return -BAD_VALUE;
    }

    videoMeta = (ExynosVideoMeta *)dstBuf->addr[videoMetaPlaneIndex];

    if (videoMeta == NULL) {
        ALOGD("ysum buffer is null");
        return INVALID_OPERATION;
    }

    videoMeta->eType = VIDEO_INFO_TYPE_YSUM_DATA;
    videoMeta->data.enc.sYsumData.high = ysumdata->higher_ysum_value;
    videoMeta->data.enc.sYsumData.low = ysumdata->lower_ysum_value;

    ALOGV("%s: higher_ysum(%ud), lower_ysum(%ud)",
          __FUNCTION__,
          videoMeta->data.enc.sYsumData.high, videoMeta->data.enc.sYsumData.low);

    return NO_ERROR;
}

#ifdef SAMSUNG_HDR10_RECORDING
status_t updateHDRBuffer(ExynosCameraBuffer *dstBuf)
{
    ExynosVideoMeta *videoMeta = NULL;
    int videoMetaPlaneIndex = dstBuf->getMetaPlaneIndex() - 1;

    if (dstBuf->size[videoMetaPlaneIndex] != EXYNOS_CAMERA_VIDEO_META_PLANE_SIZE) {
        android_printAssert(NULL, LOG_TAG,
                        "ASSERT(%s):Invalid access to video meta plane. planeIndex %d size %d",
                        __FUNCTION__, videoMetaPlaneIndex, dstBuf->size[videoMetaPlaneIndex]);
        return -BAD_VALUE;
    }

    videoMeta = (ExynosVideoMeta *)dstBuf->addr[videoMetaPlaneIndex];

    videoMeta->eType = VIDEO_INFO_TYPE_HDR_STATIC;
    videoMeta->sHdrStaticInfo.mID = 0; /* structure defined by Google, so do not know the meaning of mID. */
    videoMeta->sHdrStaticInfo.sType1.mR.x = 140;
    videoMeta->sHdrStaticInfo.sType1.mR.y = 150;

    videoMeta->sHdrStaticInfo.sType1.mG.x = 160;
    videoMeta->sHdrStaticInfo.sType1.mG.y = 170;

    videoMeta->sHdrStaticInfo.sType1.mB.x = 180;
    videoMeta->sHdrStaticInfo.sType1.mB.y = 190;

    videoMeta->sHdrStaticInfo.sType1.mW.x = 80;
    videoMeta->sHdrStaticInfo.sType1.mW.y = 90;

    videoMeta->sHdrStaticInfo.sType1.mMaxDisplayLuminance = 100;
    videoMeta->sHdrStaticInfo.sType1.mMinDisplayLuminance = 50;
    videoMeta->sHdrStaticInfo.sType1.mMaxContentLightLevel = 6;
    videoMeta->sHdrStaticInfo.sType1.mMaxFrameAverageLightLevel = 7;

    ALOGV("mId(%d), mR(x:%d, y:%d), mG(x:%d, y:%d), mB(x:%d, y:%d), mW(x:%d, y:%d)",
            videoMeta->sHdrStaticInfo.mID,
            videoMeta->sHdrStaticInfo.sType1.mR.x, videoMeta->sHdrStaticInfo.sType1.mR.y,
            videoMeta->sHdrStaticInfo.sType1.mG.x, videoMeta->sHdrStaticInfo.sType1.mG.y,
            videoMeta->sHdrStaticInfo.sType1.mB.x, videoMeta->sHdrStaticInfo.sType1.mB.y,
            videoMeta->sHdrStaticInfo.sType1.mW.x, videoMeta->sHdrStaticInfo.sType1.mW.y);
    ALOGV("DisplayLuminance(max: %d, min: %d), MaxContentLightLevel(%d), MaxFrameAverageLightLevel(%d)",
            videoMeta->sHdrStaticInfo.sType1.mMaxDisplayLuminance,
            videoMeta->sHdrStaticInfo.sType1.mMinDisplayLuminance,
            videoMeta->sHdrStaticInfo.sType1.mMaxContentLightLevel,
            videoMeta->sHdrStaticInfo.sType1.mMaxFrameAverageLightLevel);

    return NO_ERROR;
}
#endif /* SAMSUNG_HDR10_RECORDING */

void getV4l2Name(char* colorName, size_t length, int colorFormat)
{
    size_t index = 0;
    if (colorName == NULL) {
        ALOGE("colorName is NULL");
        return;
    }

    for (index = 0; index < length-1; index++) {
        colorName[index] = colorFormat & 0xff;
        colorFormat = colorFormat >> 8;
    }
    colorName[index] = '\0';
}

void setPreviewProperty(bool on)
{
    int ret = 0;
    char value[5] = {0};

    snprintf(value, sizeof(value), "%d", (on == true) ? 2 : 0);

    ALOGD("DEBUG(%s[%d]):Set persist.sys.camera.preview %s",
            __FUNCTION__, __LINE__, value);

    ret = property_set("persist.sys.camera.preview", value);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):Failed to set persist.sys.camera.preview. ret %d",
                __FUNCTION__, __LINE__, ret);
    }
}

#ifdef CAMERA_GED_FEATURE
bool isFastenAeStableSupported(int cameraId)
{
    bool ret = false;

    if (cameraId == CAMERA_ID_BACK) {
        ret = USE_FASTEN_AE_STABLE;
    }
#ifdef USE_DUAL_CAMERA
    else if (cameraId == CAMERA_ID_BACK_1) {
        ret = USE_FASTEN_AE_STABLE;
    }
#endif
    else if (cameraId == CAMERA_ID_FRONT) {
        ret = USE_FASTEN_AE_STABLE_FRONT;
    }
    else {
        ALOGE("ERR(%s[%d]): unknown cameraID(%d)", __FUNCTION__, __LINE__, cameraId);
    }

    ALOGI("INFO(%s[%d]): cameraID(%d) USE_FASTEN_AE_STABLE (%s)", __FUNCTION__, __LINE__, cameraId, ret ? "enable" : "disable");

    return ret;
}
#endif

}; /* namespace android */

#ifdef USE_INTERNAL_ALLOC_DEBUG
//NOTE: Can't use the default C++ map and list utilities incase of global new/delete operator overloading
using namespace std;
#define PRINT_CALLSTACK

static int alloc_cnt = 0;
android::Mutex              m_myAllocLock;
android::Mutex             m_AllocLock;


typedef struct myElm myElm_t;
struct myElm {
    void *ptr;
    int size;
    int cnt;
};

typedef struct my_node my_node_t;
struct my_node {
    my_node_t *next;
    myElm_t elm;
};

my_node_t *used_list = NULL;
my_node_t *free_list = NULL;
static int used_cnt = 0;
static int myList_init = 0;

static int elm_size()
{
    return alloc_cnt;
}


my_node_t *get_node()
{
    my_node_t *node;
    node = (my_node_t *)malloc(sizeof(my_node_t));
    if (!node) {
        ALOGE("[EXYNOS_CAMERA_ALLOC: %s:  %d], failed to allocate", __FUNCTION__, __LINE__);
        return NULL;
    }

    node->next = NULL;
    node->elm.ptr = NULL;
    node->elm.size = 0;
    return node;
}

void insert_node(my_node_t **list, my_node_t *node)
{
    m_myAllocLock.lock();
    if (*list == NULL) {
        *list = node;
        goto insert_node_end;
    }

    node->next = *list;
    *list = node;

insert_node_end:
    m_myAllocLock.unlock();
}


my_node_t *delete_node(my_node_t *node, my_node_t *prev)
{

    my_node_t *ret_node = NULL;
    if (node == NULL) {
        return NULL;
    }

    if (prev != NULL) {
        prev->next = node->next;
        ret_node = prev;
    } else {
        ret_node = node->next;
    }

    free(node);
    return ret_node;
}

my_node_t *remove_node(my_node_t **list, void* ptr)
{
    my_node_t *node;
    my_node_t *ret_node;
    my_node_t *prev = NULL;
    myElm_t *elm = NULL;

    m_myAllocLock.lock();
    node = *list;
    if (node && node->elm.ptr == ptr) {
        *list = node->next;
        goto remove_node_end;
    }

    while (node && node->elm.ptr != ptr) {
        prev = node;
        node = node->next;
    }

    if (node == NULL) {
        ALOGE("[EXYNOS_CAMERA_ALLOC: %s : %d]: failed to find the elm->ptr[%p] to remove : alloc_cnt = %d",
            __FUNCTION__, __LINE__, ptr, alloc_cnt);
    } else if (prev) {
        prev->next = node->next;
        node->next = NULL;
    }
remove_node_end:

    m_myAllocLock.unlock();
    return node;
}

my_node_t *get_free_node(my_node_t **list)
{
    my_node_t *node;
    myElm_t *elm = NULL;

    m_myAllocLock.lock();
    node = *list;
    if (!node) {
        node = get_node();
        if (!node)
            goto get_free_node_end;
    }
    *list = node->next;
    node->next = NULL;
get_free_node_end:

    m_myAllocLock.unlock();

    return node;
}

int alloc_info_print(int flag)
{
    my_node_t *node;
    m_myAllocLock.lock();

    ALOGI("[EXYNOS_CAMERA_ALLOC: %s : %d] : ALIVE MEMORY BLOCKS INFO -Start",
        __FUNCTION__, __LINE__);
    ALOGI("[EXYNOS_CAMERA_ALLOC: %s : %d] : %p : alloc_cnt %d used_cnt %d",
        __FUNCTION__, __LINE__, used_list, alloc_cnt, used_cnt);

    if (flag == 0) {
        node = used_list;
    } else {
        node = free_list;
    }

    while(node) {
        ALOGI("[EXYNOS_CAMERA_ALLOC: %s : %d] : %p : %d : %d", __FUNCTION__, __LINE__,
            node->elm.ptr, node->elm.size, node->elm.cnt);
        node = node->next;
    }

    ALOGI("[EXYNOS_CAMERA_ALLOC: %s : %d] : ALIVE MEMORY BLOCKS INFO -End",
        __FUNCTION__, __LINE__);
    m_myAllocLock.unlock();
    return 0;
}

int print_stack() {
#ifdef PRINT_CALLSTACK
    android::CallStack stack;
    stack.update();
    stack.log("ExynosCamera", ANDROID_LOG_ERROR, "ExynosCamera_CallStack");
#endif
    return 0;
}

void * operator new(size_t size)
{
    int cnt = 0;
    my_node_t *node;

#ifdef ALLOC_INFO_DUMP
    ALOGV("[EXYNOS_CAMERA_ALLOC: %s++ operator overloading: %d] %d : cnt = %d",
    __FUNCTION__, __LINE__, size, elm_size());
#endif

    android::Mutex::Autolock lock(m_AllocLock);
    void * p = malloc(size);
    if (!p) {
        ALOGE("[EXYNOS_CAMERA_ALLOC: %s : %d] Failed to allocate", __FUNCTION__, __LINE__);
    } else {
        alloc_cnt++;
        node = get_free_node(&free_list);
        if (node) {
            node->elm.ptr = p;
            node->elm.size = size;
            node->elm.cnt = elm_size();
            node->next = NULL;
            insert_node(&used_list, node);
            used_cnt++;
        } else {
            ALOGE("[EXYNOS_CAMERA_ALLOC: %s : %d] Failed to get node for storing the alloc info : %d  %d",
                __FUNCTION__, __LINE__, alloc_cnt, used_cnt);
        }
    }
#ifdef ALLOC_INFO_DUMP
    ALOGI("[EXYNOS_CAMERA_ALLOC: %s-- operator overloading: %d] %p : %d : cnt = %d",
    __FUNCTION__, __LINE__, p, size, elm_size());
#endif
    return p;
}

void operator delete(void * p)
{
    my_node_t *node;
#ifdef ALLOC_INFO_DUMP
    ALOGV("[EXYNOS_CAMERA_ALLOC: %s++ operator overloading: %d] %p : %d",
    __FUNCTION__, __LINE__, p, elm_size());
#endif

    android::Mutex::Autolock lock(m_AllocLock);
    free(p);
    alloc_cnt--;
    node = remove_node(&used_list, p);
    if (node) {
        used_cnt--;
        node->elm.ptr = NULL;
        node->elm.size = 0;
        node->elm.cnt = elm_size();
        node->next = NULL;
        insert_node(&free_list, node);
    } else {
        ALOGE("[EXYNOS_CAMERA_ALLOC: %s : %d] Seems..the pointer is invalid or already freed : [%p] : %d  %d",
            __FUNCTION__, __LINE__, p, alloc_cnt, used_cnt);
        print_stack();
        alloc_info_print(0);
        //assert(false);
    }
#ifdef ALLOC_INFO_DUMP
    ALOGI("[EXYNOS_CAMERA_ALLOC : %s-- operator overloading: %d] %p : %d",
    __FUNCTION__, __LINE__, p, elm_size());
#endif
    return;
}

void * operator new[](size_t size)
{
    int cnt = 0;
    my_node_t *node;

#ifdef ALLOC_INFO_DUMP
    ALOGV("[EXYNOS_CAMERA_ALLOC: %s++ operator overloading: %d] %d : cnt = %d",
    __FUNCTION__, __LINE__, size, elm_size());
#endif

    android::Mutex::Autolock lock(m_AllocLock);
    void * p = malloc(size);
    if (!p) {
        ALOGE("[EXYNOS_CAMERA_ALLOC: %s : %d] Failed to allocate", __FUNCTION__, __LINE__);
    } else {
        alloc_cnt++;
        node = get_free_node(&free_list);
        if (node) {
            node->elm.ptr = p;
            node->elm.size = size;
            node->elm.cnt = elm_size();
            node->next = NULL;
            insert_node(&used_list, node);
            used_cnt++;
        } else {
            ALOGE("[EXYNOS_CAMERA_ALLOC: %s : %d] Failed to get node for storing the alloc info : %d  %d",
                __FUNCTION__, __LINE__, alloc_cnt, used_cnt);
        }
    }
#ifdef ALLOC_INFO_DUMP
    ALOGI("[EXYNOS_CAMERA_ALLOC: %s-- operator overloading: %d] %p : %d : cnt = %d",
    __FUNCTION__, __LINE__, p, size, elm_size());
#endif
    return p;
}

void operator delete[](void * p)
{
    my_node_t *node;
#ifdef ALLOC_INFO_DUMP
    ALOGV("[EXYNOS_CAMERA_ALLOC : %s++ operator overloading: %d] %p : %d",
    __FUNCTION__, __LINE__, p, elm_size());
#endif

    android::Mutex::Autolock lock(m_AllocLock);
    free(p);
    alloc_cnt--;
    node = remove_node(&used_list, p);
    if (node) {
        used_cnt--;
        node->elm.ptr = NULL;
        node->elm.size = 0;
        node->elm.cnt = elm_size();
        node->next = NULL;
        insert_node(&free_list, node);
    } else {
        ALOGE("[EXYNOS_CAMERA_ALLOC: %s : %d] Seems..the pointer is invalid or already freed : [%p] : %d  %d",
            __FUNCTION__, __LINE__, p, alloc_cnt, used_cnt);
        print_stack();
        alloc_info_print(0);
        //assert(false);
    }
#ifdef ALLOC_INFO_DUMP
    ALOGI("[EXYNOS_CAMERA_ALLOC: %s-- operator overloading: %d] %p : %d",
    __FUNCTION__, __LINE__, p, elm_size());
#endif
    return;
}

#endif
