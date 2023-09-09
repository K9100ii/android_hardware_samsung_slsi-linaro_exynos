/*
 * Copyright (C) 2014, Samsung Electronics Co. LTD
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

//#define LOG_NDEBUG 0
#define LOG_TAG "ExynosCameraMetadataConverterVendor"

#include "ExynosCameraMetadataConverter.h"

namespace android {

void ExynosCamera3MetadataConverter::m_constructVendorDefaultRequestSettings(__unused int type, __unused CameraMetadata *settings)
{
    return;
}

void ExynosCamera3MetadataConverter::m_constructVendorStaticInfo(__unused struct ExynosSensorInfoBase *sensorStaticInfo, __unused CameraMetadata *info)
{
    return;
}

void ExynosCamera3MetadataConverter::translateVendorControlControlData(__unused CameraMetadata *settings,
                                                                    __unused struct camera2_shot_ext *dst_ext)
{
    return;
}

void ExynosCamera3MetadataConverter::translateVendorControlMetaData(__unused CameraMetadata *service_settings,
                                                                   __unused CameraMetadata *settings,
                                                                   __unused struct camera2_shot_ext *src_ext)
{
    return;
}

void ExynosCamera3MetadataConverter::translateVendorLensControlData(__unused CameraMetadata *settings,
                                                                    __unused struct camera2_shot_ext *dst_ext)
{
    return;
}

void ExynosCamera3MetadataConverter::translateVendorLensMetaData(__unused CameraMetadata *settings,
                                                                 __unused struct camera2_shot_ext *src_ext,
                                                                 __unused struct camera2_shot_ext *service_settings)
{
    return;
}

void ExynosCamera3MetadataConverter::translateVendorPartialMetaData(__unused CameraMetadata *settings,
                                                                   __unused struct camera2_shot_ext *src_ext)
{
    return;
}
}; /* namespace android */
