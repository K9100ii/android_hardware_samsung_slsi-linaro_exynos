/*
 * Copyright (C) 2017, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_METADATA_CONVERTER_H__
#define EXYNOS_CAMERA_METADATA_CONVERTER_H__

#include <log/log.h>
#include <utils/RefBase.h>
#include <hardware/camera3.h>
#include <CameraMetadata.h>

#include "ExynosCameraConfig.h"
#include "ExynosCameraParameters.h"
#include "ExynosCameraSensorInfo.h"
#include "fimc-is-metadata.h"

#define FIMC_IS_METADATA(x) (x + 1)
#define CAMERA_METADATA(x)  ((x < 1)? 0 : x - 1)

#define PREVIEW_FORMAT_MIN_DURATION         (33331760L)
#define PICTURE_FORMAT_MIN_DURATION         (100000000L)
#define ACTUAL_PIPELINE_DEPTH               (4)
#define FRAMECOUNT_MAP_LENGTH               (100)

namespace android {

class ExynosCameraRequestManager;
class ExynosCameraRequest;

typedef sp<ExynosCameraRequest> ExynosCameraRequestSP_sprt_t;
typedef sp<ExynosCameraRequest>& ExynosCameraRequestSP_dptr_t;

enum map_index {
    CAMERA_META,
    FIMC_IS_META,
    MAX_INDEX
};
const int32_t PREVIEW_FORMATS[] =
{
    HAL_PIXEL_FORMAT_RGBA_8888,
    HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
    HAL_PIXEL_FORMAT_YCbCr_420_888
};

const int32_t PICTURE_FORMATS[] =
{
    /* HAL_PIXEL_FORMAT_RAW_SENSOR, */
    HAL_PIXEL_FORMAT_RAW16,
    HAL_PIXEL_FORMAT_BLOB,
};

enum rectangle_index {
    X1,
    Y1,
    X2,
    Y2,
    RECTANGLE_MAX_INDEX,
};

enum face_landmarks_index {
    LEFT_EYE_X,
    LEFT_EYE_Y,
    RIGHT_EYE_X,
    RIGHT_EYE_Y,
    MOUTH_X,
    MOUTH_Y,
    FACE_LANDMARKS_MAX_INDEX,
};

enum frame_count_map_item_index {
    TIMESTAMP,
    FRAMECOUNT,
    APERTURE,
    FRAME_COUNT_MAP_ITEM_MAX_INDEX,
};

enum metadata_type {
    PARTIAL_NONE,
    PARTIAL_3AA,
    PARTIAL_JPEG,
    PARTIAL_MAX,
};

class ExynosCameraMetadataConverter : public virtual RefBase {
public:
    ExynosCameraMetadataConverter(int cameraId, ExynosCameraParameters *parameters);
    ~ExynosCameraMetadataConverter();
    static  status_t        constructStaticInfo(int cameraId, camera_metadata_t **info);
    virtual status_t        constructDefaultRequestSettings(int type, camera_metadata_t **request);

    /* helper functions for android control and meta data */
    virtual status_t        convertRequestToShot(ExynosCameraRequestSP_sprt_t request, int *reqId = NULL);
    virtual status_t        updateDynamicMeta(ExynosCameraRequestSP_sprt_t requestInfo, enum metadata_type metaType);
    virtual void            setPreviousMeta(CameraMetadata *meta);

    /* meta -> shot */
    virtual status_t        translateColorControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    virtual status_t        translateControlControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext,
							struct CameraMetaParameters *metaParameters);
    virtual status_t        translateDemosaicControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    virtual status_t        translateEdgeControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    virtual status_t        translateFlashControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    virtual status_t        translateHotPixelControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    virtual status_t        translateJpegControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    virtual status_t        translateLensControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext,
                                                     struct CameraMetaParameters *metaParameters);
    virtual status_t        translateNoiseControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    virtual status_t        translateRequestControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext, int *reqId);
    virtual status_t        translateScalerControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext,
		                                        struct CameraMetaParameters *metaParameters);
    virtual status_t        translateSensorControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    virtual status_t        translateShadingControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    virtual status_t        translateStatisticsControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    virtual status_t        translateTonemapControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    virtual status_t        translateLedControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    virtual status_t        translateBlackLevelControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);

    /* shot -> meta */
    virtual status_t        translateColorMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateControlMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateEdgeMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateFlashMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateHotPixelMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateJpegMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateLensMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateNoiseMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateRequestMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateScalerMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateSensorMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateShadingMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateStatisticsMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateTonemapMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateLedMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translateBlackLevelMetaData(ExynosCameraRequestSP_sprt_t requestInfo);
    virtual status_t        translatePartialMetaData(ExynosCameraRequestSP_sprt_t requestInfo, enum metadata_type metaType);

    /* vendor functions */
    /* control meta */
#ifdef SAMSUNG_TN_FEATURE
    void                    translatePreVendorControlControlData(CameraMetadata *settings,
                                                                  struct camera2_shot_ext *dst_ext);
    void                    translateVendorControlControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    void                    translateVendorAeControlControlData(struct camera2_shot_ext *dst_ext, uint32_t vendorAeMode,
                                                                 aa_aemode *aeMode,
                                                                 ExynosCameraActivityFlash::FLASH_REQ *flashReq,
                                                                 struct CameraMetaParameters *metaParameters);
    void                    translateVendorAfControlControlData(struct camera2_shot_ext *dst_ext, uint32_t vendorAfMode);
    void                    translateVendorLensControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext,
		                                            struct CameraMetaParameters *metaParameters);
    void                    translateVendorSensorControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    void                    translateVendorScalerControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);
    void                    translateVendorLedControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext);

    /* dynamic meta */
    void                    translateVendorControlMetaData(CameraMetadata *settings, struct camera2_shot_ext *src_ext);
    void                    translateVendorJpegMetaData(CameraMetadata *settings);
    void                    translateVendorLensMetaData(CameraMetadata *settings, struct camera2_shot_ext *src_ext);
    void                    translateVendorSensorMetaData(CameraMetadata *settings, struct camera2_shot_ext *src_ext);
    void                    translateVendorPartialLensMetaData(CameraMetadata *settings, struct camera2_shot_ext *src_ext);
    void                    translateVendorPartialControlMetaData(CameraMetadata *settings, struct camera2_shot_ext *src_ext);
    void                    translateVendorPartialMetaData(CameraMetadata *settings, struct camera2_shot_ext *src_ext, enum metadata_type metaType);
#endif

    /* Other helper functions */
    virtual status_t        initShotData(struct camera2_shot_ext *shot_ext);
    status_t                initShotVendorData(struct camera2_shot *shot);
    virtual status_t        checkAvailableStreamFormat(int format);

    virtual void            setStaticInfo(int camId, camera_metadata_t *info);
    virtual status_t        checkMetaValid(camera_metadata_tag_t tag, const void *data);
#ifdef SAMSUNG_TN_FEATURE
    virtual status_t        checkRangeOfValid(int32_t tag, int32_t value);
#endif
    virtual status_t        getDefaultSetting(camera_metadata_tag_t tag, void *data);
private:
    static status_t         m_createAvailableCapabilities(const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                          Vector<uint8_t> *capabilities);
    static status_t         m_createAvailableKeys(const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                 Vector<int32_t> *request, Vector<int32_t> *result, Vector<int32_t> *characteristics,
                                                 int cameraId);
    static status_t         m_createControlAvailableHighSpeedVideoConfigurations(const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                                                 Vector<int32_t> *streamConfigs);
    static status_t         m_createScalerAvailableInputOutputFormatsMap(const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                                         Vector<int32_t> *streamConfigs);
    static status_t         m_createScalerAvailableStreamConfigurationsOutput(const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                                              Vector<int32_t> *streamConfigs);
    static status_t         m_createScalerAvailableMinFrameDurations(const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                                     Vector<int64_t> *minDurations);
    static status_t         m_createJpegAvailableThumbnailSizes(const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                                Vector<int32_t> *thumbnailSizes);
    static status_t         m_createAeAvailableFpsRanges(const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                         Vector<int32_t> *fpsRanges);
    static status_t         m_createVendorControlAvailableVideoConfigurations(
                                                         const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                         Vector<int32_t> *streamConfigs);
    static status_t         m_createVendorControlAvailableHighSpeedVideoConfigurations(
                                                         const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                         Vector<int32_t> *streamConfigs);
    static status_t         m_createVendorControlAvailableAeModeConfigurations(
                                                         const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                         Vector<uint8_t> *vendorAeModes);
    static status_t         m_createVendorControlAvailableAfModeConfigurations(
                                                         const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                         Vector<uint8_t> *vendorAfModes);
#ifdef SUPPORT_DEPTH_MAP
    static status_t         m_createVendorDepthAvailableDepthConfigurations(
                                                         const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                         Vector<int32_t> *streamConfigs);
#endif

    static status_t         m_createVendorScalerAvailableThumbnailConfigurations(
                                                         const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                         Vector<int32_t> *streamConfigs);

    static status_t         m_createVendorControlAvailableEffectModesConfigurations(
                                                         const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                         Vector<uint8_t> *vendorEffectModes);

    static bool             m_hasTagInList(int32_t *list, size_t listSize, int32_t tag);
    static bool             m_hasTagInList(uint8_t *list, size_t listSize, int32_t tag);
    static status_t         m_integrateOrderedSizeList(int (*list1)[SIZE_OF_RESOLUTION], size_t list1Size,
                                                       int (*list2)[SIZE_OF_RESOLUTION], size_t list2Size,
                                                       int (*orderedList)[SIZE_OF_RESOLUTION]);

    void                    m_updateFaceDetectionMetaData(CameraMetadata *settings,
                                                          struct camera2_shot_ext *shot_ext);
    void                    m_convertActiveArrayTo3AARegion(ExynosRect2 *region);
    void                    m_convert3AAToActiveArrayRegion(ExynosRect2 *region);
#ifdef SAMSUNG_TN_FEATURE
    static void             m_constructVendorStaticInfo(struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                        CameraMetadata *info, int cameraId);
#endif
    void                    m_constructVendorDefaultRequestSettings(int type, CameraMetadata *settings);
    void                    setMetaCtlSceneMode(struct camera2_shot_ext *shot_ext, enum aa_scene_mode sceneMode, int mode = 0);
#ifdef SAMSUNG_TN_FEATURE
    void                    setShootingMode(int shotMode, struct camera2_shot_ext *dst_ext);
#endif
    void                    setSceneMode(int value, struct camera2_shot_ext *dst_ext);
    uint32_t                m_getFrameInfoForTimeStamp(enum frame_count_map_item_index index, uint64_t timeStamp);

private:
    int                             m_cameraId;
    char                            m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
    ExynosCameraParameters         *m_parameters;
    ExynosCameraActivityFlash       *m_flashMgr;

    CameraMetadata                  m_staticInfo;
    CameraMetadata                  m_defaultRequestSetting;
    CameraMetadata                  *m_prevMeta;
    struct ExynosCameraSensorInfoBase *m_sensorStaticInfo;

    int                             m_frameCountMapIndex;
    uint64_t                        m_frameCountMap[FRAMECOUNT_MAP_LENGTH][FRAME_COUNT_MAP_ITEM_MAX_INDEX];

    bool                            m_preCaptureTriggerOn;
    bool                            m_isManualAeControl;
    uint32_t                        m_maxFps;
    bool                            m_overrideFlashControl;
    uint8_t                         m_defaultAntibanding;
    uint32_t                        m_afMode;
    uint32_t                        m_preAfMode;
    float                           m_focusDistance;
#ifdef SUPPORT_MULTI_AF
    bool                            m_flagMultiAf;
#endif
    int                             m_sceneMode;
    int                             m_aemode; //for iris test
    int                             m_aperture; //for iris test
};

}; /* namespace android */
#endif
