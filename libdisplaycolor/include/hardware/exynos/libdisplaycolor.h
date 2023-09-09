/*
 *   Copyright 2020 Samsung Electronics Co., Ltd.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
#ifndef __LIB_DISPLAY_COLOR_INTERFACE_H__
#define __LIB_DISPLAY_COLOR_INTERFACE_H__

#include <system/graphics.h>
#include <string>
#include <vector>

struct DisplayColorMode {
    uint32_t modeId;
    uint32_t gamutId;
    std::string modeName;
};

struct DisplayRenderIntent {
    uint32_t intentId;
    std::string intentName;
};

class IDisplayColor {
    public:
        virtual ~IDisplayColor() {}
        static IDisplayColor *createInstance(uint32_t __attribute__((unused)) display_id);
        virtual std::vector<DisplayColorMode> getColorModes() = 0;
        virtual int setColorMode(uint32_t __attribute__((unused)) mode) {return 0;}
        virtual std::vector<DisplayRenderIntent> getRenderIntents(uint32_t __attribute__((unused)) mode) = 0;
        virtual int setColorModeWithRenderIntent(uint32_t __attribute__((unused)) mode, uint32_t __attribute__((unused)) intent) {return 0;}
        virtual int setColorTransform(const float __attribute__((unused)) *matrix, uint32_t __attribute__((unused)) hint) {return 0;}
        virtual int getDqeLutSize() {return 0;}
        virtual int getDqeLut(void __attribute__((unused)) *parcel) {return 0;}
        virtual void setLogLevel(int __attribute__((unused)) log_level) {}
};

#endif /* __LIB_DISPLAY_COLOR_INTERFACE_H__ */
