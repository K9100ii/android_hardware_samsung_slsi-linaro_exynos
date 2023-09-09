/*
 *  libhdr-common-header/hdr10pMetaInterface.h
 *
 *   Copyright 2018 Samsung Electronics Co., Ltd.
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

#ifndef __HDR10P_META_INTERFACE_H__
#define __HDR10P_META_INTERFACE_H__

enum Hdr10pErrFlag {
    HDR10P_ERR_NO = 0,	/* OK - NO ERROR */
    HDR10P_ERR_INVAL,	/* INVALID VALUE */
    HDR10P_ERR_PTR,	/* POINTER ERROR */
    HDR10P_ERR_NOPERM,	/* NOT PERMITTED */
    HDR10P_ERR_MAX,
};

class hdr10pMetaInterface {
public:
    virtual ~hdr10pMetaInterface() {}
    static hdr10pMetaInterface *createInstance(void);
    virtual int meta2meta(void __attribute__((unused)) *in_metadata, unsigned int __attribute__((unused)) in_len,
		    unsigned int __attribute__((unused)) targetMaxLuminance,
		    void __attribute__((unused)) *out_metadata, unsigned int __attribute__((unused)) out_len) { return 0; }
    virtual void setLogLevel(int __attribute__((unused)) log_level) {}
};

#endif /* __HDR10P_META_INTERFACE_H__ */
