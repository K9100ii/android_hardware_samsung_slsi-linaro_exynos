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
#define LOG_TAG "ExynosCameraPlugInConverterFakeFusion"

#include "ExynosCameraPlugInConverterFakeFusion.h"

namespace android {

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugInConverterFakeFusion::m_init(void)
{
    strncpy(m_name, "ConverterFakeFusion", (PLUGIN_NAME_STR_SIZE - 1));

    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterFakeFusion::m_deinit(void)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterFakeFusion::m_create(__unused Map_t *map)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterFakeFusion::m_setup(__unused Map_t *map)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterFakeFusion::m_make(__unused Map_t *map)
{
    return NO_ERROR;
}
}; /* namespace android */
