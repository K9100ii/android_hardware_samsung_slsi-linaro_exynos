/*
 * Copyright@ Samsung Electronics Co. LTD
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

#define LOG_TAG "ExynosCameraProperty"

#include <string>
/* #include <utils/Log.h> */
#include <cutils/properties.h>

#include "ExynosCameraProperty.h"

const ExynosCameraProperty::Property_t ExynosCameraProperty::kMap[MAX_NUM_PROPERTY] = {
    /* "prop key", "type", "needTag", "needSplit", "default_value" */
    {.key = {"log.tag"},                                ExynosCameraProperty::TYPE_STRING, true,  false, { .s = "D" }},         //LOG_TAG
    {.key = {"persist.log.tag"},                        ExynosCameraProperty::TYPE_STRING, true,  false, { .s = "D" }},         //PERSIST_LOG_TAG
    {.key = {"sys.camera.log.perframe.enable"},         ExynosCameraProperty::TYPE_BOOL,   false, false, { .b = false }},       //LOG_PERFRAME_ENABLE
    {.key = {"sys.camera.log.perframe.filter.camId"},   ExynosCameraProperty::TYPE_INT32,  false, true,  { .s = "" }},          //LOG_PERFRAME_FILTER_CAMID
    {.key = {"sys.camera.log.perframe.filter.factory"}, ExynosCameraProperty::TYPE_STRING, false, true,  { .s = "" }},          //LOG_PERFRAME_FILTER_FACTORY
    {.key = {"sys.camera.log.perframe.size.enable"},    ExynosCameraProperty::TYPE_BOOL,   false, false, { .b = false }},       //LOG_PERFRAME_SIZE_ENABLE
    {.key = {"sys.camera.log.perframe.buf.enable"},     ExynosCameraProperty::TYPE_BOOL,   false, false, { .b = false }},       //LOG_PERFRAME_BUF_ENABLE
    {.key = {"sys.camera.log.perframe.buf.filter"},     ExynosCameraProperty::TYPE_STRING, false, true,  { .s = "" }},          //LOG_PERFRAME_BUF_FILTER
    {.key = {"sys.camera.log.perframe.meta.enable"},    ExynosCameraProperty::TYPE_BOOL,   false, false, { .b = false }},       //LOG_PERFRAME_META_ENABLE
    {.key = {"sys.camera.log.perframe.meta.filter"},    ExynosCameraProperty::TYPE_STRING, false, true,  { .s = "" }},          //LOG_PERFRAME_META_FILTER
    {.key = {"sys.camera.log.perframe.meta.values"},    ExynosCameraProperty::TYPE_STRING, false, true,  { .s = "" }},          //LOG_PERFRAME_META_VALUES
    {.key = {"sys.camera.log.perframe.path.enable"},    ExynosCameraProperty::TYPE_BOOL,   false, false, { .b = false }},       //LOG_PERFRAME_PATH_ENABLE
    {.key = {"sys.camera.log.performance.enable"},      ExynosCameraProperty::TYPE_BOOL,   false, false, { .b = false }},       //LOG_PERFORMANCE_ENABLE
    {.key = {"sys.camera.log.performance.fps.enable"},  ExynosCameraProperty::TYPE_BOOL,   false, false, { .b = false }},       //LOG_PERFORMANCE_FPS_ENABLE
    {.key = {"sys.camera.log.performance.fps.frames"},  ExynosCameraProperty::TYPE_INT32,  false, false, { .i32 = 60 }},        //LOG_PERFORMANCE_FPS_FRAMES
    {.key = {"sys.camera.debug.assert"},                ExynosCameraProperty::TYPE_STRING, false, false, { .s = "normal" }},    //DEBUG_ASSERT
    {.key = {"sys.camera.debug.trap.enable"},           ExynosCameraProperty::TYPE_BOOL,   false, false, { .b = false }},       //DEBUG_TRAP_ENABLE
    {.key = {"sys.camera.debug.trap.words"},            ExynosCameraProperty::TYPE_STRING, false, true,  { .s = "" }},          //DEBUG_TRAP_WORDS
    {.key = {"sys.camera.debug.trap.event"},            ExynosCameraProperty::TYPE_STRING, false, false, { .s = "panic" }},     //DEBUG_TRAP_EVENT
    {.key = {"sys.camera.debug.trap.count"},            ExynosCameraProperty::TYPE_INT32,  false, false, { .i32 = 1 }},         //DEBUG_TRAP_COUNT
    {.key = {"sys.camera.tuning.obte.enable"},          ExynosCameraProperty::TYPE_BOOL,   false, false, { .b = false }},       //TUNING_OBTE_ENABLE
};

/*
 * Class ExynosCameraProperty
 */
ExynosCameraProperty::ExynosCameraProperty()
{
    strncpy(m_name, "Property", (EXYNOS_CAMERA_NAME_STR_SIZE - 1));
}

ExynosCameraProperty::~ExynosCameraProperty()
{}

const char * ExynosCameraProperty::m_getPropKey(PropMap mapKey, const char *tag, std::string &str)
{
    if (!kMap[mapKey].needTag)
        return kMap[mapKey].key;

    str.append(kMap[mapKey].key);
    str.append(".");
    str.append(tag);

    return str.c_str();
}

status_t ExynosCameraProperty::set(PropMap mapKey, const char *tag, const char *value)
{
    std::string str;
    const char* propKey = m_getPropKey(mapKey, tag, str);

    return property_set(propKey, value);
}

status_t ExynosCameraProperty::m_get(PropMap mapKey, const char *propKey, bool &value)
{
    if (m_checkPropType(mapKey, TYPE_BOOL)) return INVALID_OPERATION;

    value = property_get_bool(propKey, kMap[mapKey].default_value.b);

    return NO_ERROR;
}

status_t ExynosCameraProperty::m_get(PropMap mapKey, const char *propKey, int32_t &value)
{
    if (m_checkPropType(mapKey, TYPE_INT32)) return INVALID_OPERATION;

    value = property_get_int32(propKey, kMap[mapKey].default_value.i32);

    return NO_ERROR;
}

status_t ExynosCameraProperty::m_get(PropMap mapKey, const char *propKey, int64_t &value)
{
    if (m_checkPropType(mapKey, TYPE_INT64)) return INVALID_OPERATION;

    value = property_get_int64(propKey, kMap[mapKey].default_value.i64);

    return NO_ERROR;
}

status_t ExynosCameraProperty::m_get(PropMap mapKey, const char *propKey, double &value)
{
    if (m_checkPropType(mapKey, TYPE_DOUBLE)) return INVALID_OPERATION;

    char property[PROPERTY_VALUE_MAX];

    int len = property_get(propKey, property, "0.0");
    if (len <= 0) {
        value = kMap[mapKey].default_value.d;
    } else {
        value = std::stold(property);
    }

    return NO_ERROR;
}

status_t ExynosCameraProperty::m_get(PropMap mapKey, const char *propKey, std::string &value)
{
    char property[PROPERTY_VALUE_MAX];

    int len = property_get(propKey, property, kMap[mapKey].default_value.s);
    if (len <= 0) {
        value.clear();
    } else {
        value.assign(property);
    }

    return NO_ERROR;
}
