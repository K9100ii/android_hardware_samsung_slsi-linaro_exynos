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

/*!
 * \file      ExynosCameraProperty.h
 * \brief     header file for ExynosCameraProperty
 * \author    Teahyung, Kim(tkon.kim@samsung.com)
 * \date      2018/05/08
 *
 * <b>Revision History: </b>
 * - 2018/05/08 : Teahyung, Kim(tkon.kim@samsung.com) \n
 */

#ifndef EXYNOS_CAMERA_PROPERTY_H
#define EXYNOS_CAMERA_PROPERTY_H

#ifdef USE_DEBUG_PROPERTY

#include <regex>
#include <string>
#include <vector>
#include <utils/Errors.h>

#include "ExynosCameraObject.h"

using namespace android;

/**
 * ExynosCameraProperty is to manage CameraHAL's property.
 * So user can only set or get the property supported by this class.
 * This class provides several API to manage property easily.
 *
 * ex. ExynosCameraProperty property;
 *     std::string propValue;
 *     ret = m_property.get(ExynosCameraProperty::PERSIST_LOG_TAG, tag, propValue);
 *     if (ret != NO_ERROR) return;
 *     if (propValue.length() == 0 || propValue.compare("D") == 0) return;
 * ex. ExynosCameraProperty property;
 *     std::vector<int32_t> i32V;
 *     ret = property.getVector(ExynosCameraProperty::LOG_PERFRAME_FILTER_CAMID,
 *          tag, i32V);
 *     for(int32_t each : i32V) {
 *          ALOGD("each: %d", each);
 *     }
 *
 */
class ExynosCameraProperty : public ExynosCameraObject
{
public:
    typedef enum {
        _LOG_TAG,
        PERSIST_LOG_TAG,
        LOG_PERFRAME_ENABLE,
        LOG_PERFRAME_FILTER_CAMID,
        LOG_PERFRAME_FILTER_FACTORY,
        LOG_PERFRAME_SIZE_ENABLE,
        LOG_PERFRAME_BUF_ENABLE,
        LOG_PERFRAME_BUF_FILTER,
        LOG_PERFRAME_META_ENABLE,
        LOG_PERFRAME_META_FILTER,
        LOG_PERFRAME_META_VALUES,
        LOG_PERFRAME_PATH_ENABLE,
        LOG_PERFORMANCE_ENABLE,
        LOG_PERFORMANCE_FPS_ENABLE,
        LOG_PERFORMANCE_FPS_FRAMES,
        DEBUG_ASSERT,
        DEBUG_TRAP_ENABLE,
        DEBUG_TRAP_WORDS,
        DEBUG_TRAP_EVENT,
        DEBUG_TRAP_COUNT,
        TUNING_OBTE_ENABLE,
        MAX_NUM_PROPERTY,
    } PropMap;

    typedef enum {
        TYPE_BOOL,
        TYPE_INT32,
        TYPE_INT64,
        TYPE_DOUBLE,
        TYPE_STRING,
        TYPE_MAX,
    } ValueType;

    /**
     * property entry.
     * key : property string
     * type : property value's data type
     * needTag : if this field is true, final key value would be key + tag
     *          eg. "log.tag" + "ExynosCamera" = log.tag.ExynosCamera
     * needSplit : if this field is true, this property can have multiple values.
     *             multiple values can be divided by delimeter "|"
     * default_value : default value
     */
    typedef struct {
        const char key[PROPERTY_VALUE_MAX];
        ValueType type;
        bool needTag;
        bool needSplit;
        union {
            bool b;
            int32_t i32;
            int64_t i64;
            double d;
            const char s[PROPERTY_VALUE_MAX];
        } default_value;
    } Property_t;

public:
    ExynosCameraProperty();
    virtual ~ExynosCameraProperty();

public:

    /**
     * set the property value
     * @mapKey : unique key for specific property
     * @tag : LOG_TAG from caller
     * @value : value to set to property
     */
    status_t set(PropMap mapKey, const char *tag, const char* value);

    /**
     * get the single property value(template)
     * This class support several data types by ValueType enum
     * @mapKey : unique key for specific property
     * @tag : LOG_TAG from caller
     * @value : value to get single value from property
     */
    template <typename T>
    status_t get(PropMap mapKey, const char *tag, T &value)
    {
        if (kMap[mapKey].needSplit) return INVALID_OPERATION;

        std::string str;
        const char* propKey = m_getPropKey(mapKey, tag, str);

        return m_get(mapKey, propKey, value);
    }

    /**
     * get the multiple property values(template)
     * This class support several data types by ValueType enum
     * @mapKey : unique key for specific property
     * @tag : LOG_TAG from caller
     * @value : caller's vector to get multiple value from property
     */
    template <typename T>
    status_t getVector(PropMap mapKey, const char *tag, std::vector<T> &value)
    {
        if (!kMap[mapKey].needSplit) return INVALID_OPERATION;

        status_t ret;
        std::string str;
        std::string propStr;
        const char* propKey = m_getPropKey(mapKey, tag, str);

        // get all string
        ret = m_get(mapKey, propKey, propStr);

        if (ret != NO_ERROR) return ret;
        if (propStr.empty()) return ret;

        return m_getVector(propStr, value);
    }

protected:
    const char *m_getPropKey(PropMap mapKey, const char *tag, std::string &str);
    inline bool m_checkPropType(PropMap mapKey, ValueType type) { return (kMap[mapKey].type != type); }

    status_t m_get(PropMap mapKey, const char *propKey, bool &value);
    status_t m_get(PropMap mapKey, const char *propKey, char &value);
    status_t m_get(PropMap mapKey, const char *propKey, int32_t &value);
    status_t m_get(PropMap mapKey, const char *propKey, int64_t &value);
    status_t m_get(PropMap mapKey, const char *propKey, double &value);
    status_t m_get(PropMap mapKey, const char *propKey, std::string &value);

    template <typename T>
    status_t m_getVector(std::string &str, std::vector<T> &v)
    {
        const std::regex re("[|]+"); // delimeter is '|'

        std::sregex_token_iterator it(str.begin(), str.end(), re, -1);
        std::sregex_token_iterator reg_end;

        for (; it != reg_end; ++it) {
            if (it->str().length() > 0) {
                m_convertAndStoreToken(it->str(), v);
            }
        }

        return NO_ERROR;
    }

    inline void m_convertAndStoreToken(const std::string &token, std::vector<bool> &v)
    {
        if (token.compare("true") == 0) v.push_back(true);
        else                            v.push_back(false);
    }

    inline void m_convertAndStoreToken(const std::string &token, std::vector<char> &v)
    {
        v.push_back(token.at(0));
    }

    inline void m_convertAndStoreToken(const std::string &token, std::vector<int32_t> &v)
    {
        v.push_back(std::stoi(token));
    }

    inline void m_convertAndStoreToken(const std::string &token, std::vector<int64_t> &v)
    {
        v.push_back(std::stoll(token));
    }

    inline void m_convertAndStoreToken(const std::string &token, std::vector<double> &v)
    {
        v.push_back(std::stod(token));
    }

    inline void m_convertAndStoreToken(const std::string &token, std::vector<std::string> &v)
    {
        v.push_back(token);
    }

private:
    static const Property_t kMap[MAX_NUM_PROPERTY];
};

#endif //USE_DEBUG_PROPERTY
#endif //EXYNOS_CAMERA_PROPERTY_H
