/*
 *
 * Copyright 2021 Samsung Electronics S.LSI Co. LTD
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

#ifndef HDR_10PLUS_META_INTF_H_
#define HDR_10PLUS_META_INTF_H_

#include "VendorVideoAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

constexpr char LIB_FN_NAME_CREATE[]   = "CreateHdr10PlusMetaGenIntfFactory";
constexpr char LIB_FN_NAME_DESTROY[]  = "DestroyHdr10PlusMetaGenIntfFactory";

class Hdr10PlusMetaInterface {
public:
    virtual ~Hdr10PlusMetaInterface() = default;
    virtual void initHdr10PlusMeta(void) = 0;
    virtual int getHdr10PlusMetadata(ExynosHdrDynamicStatInfo* stat, ExynosHdrData_ST2094_40* meta) = 0;
    virtual void setLogLevel(int log_level) = 0;
};

typedef Hdr10PlusMetaInterface *(*CreateHdr10PlusMetaGenIntfFunc)();
typedef void (*DestroyHdr10PlusMetaGenIntfFunc)(Hdr10PlusMetaInterface *factory);

#ifdef __cplusplus
}
#endif

#endif
