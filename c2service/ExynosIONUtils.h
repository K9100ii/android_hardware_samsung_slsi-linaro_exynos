/*
 *
 * Copyright 2018 Samsung Electronics S.LSI Co. LTD
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
#ifndef EXYNOS_ION_UTILS_H
#define EXYNOS_ION_UTILS_H

#include <C2Config.h>
#include <util/C2InterfaceHelper.h>

class ExynosIONUtils {
public:
    static constexpr int32_t MAX_HEAP_NAME = 32;

    static C2R setIonUsage(C2InterfaceHelper::C2P<C2StoreIonUsageInfo> &me);
    static uint32_t getIonUsageMask();

    static C2R setDmaUsage(C2InterfaceHelper::C2P<C2StoreDmaBufUsageInfo> &me);
    static uint32_t getDmaUsageMask();

private:
    ExynosIONUtils() = delete;
};

#endif // EXYNOS_ION_UTILS_H
