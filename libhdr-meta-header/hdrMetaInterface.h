/*
 *  libhdr-meta-header/hdrMetaInterface.h
 *
 *   Copyright 2022 Samsung Electronics Co., Ltd.
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

#ifndef __HDR_META_INTERFACE_H__
#define __HDR_META_INTERFACE_H__

enum HdrMetaIfErrFlag {
    HDRMETAIF_ERR_NO = 0,	/* OK - NO ERROR */
    HDRMETAIF_ERR_INVAL,	/* INVALID VALUE */
    HDRMETAIF_ERR_PTR,          /* POINTER ERROR */
    HDRMETAIF_ERR_NOPERM,	/* NOT PERMITTED */
    HDRMETAIF_ERR_MAX,
};

class hdrMetaInterface {
public:
    virtual ~hdrMetaInterface() {}
    static hdrMetaInterface *createInstance(void);
/*
 *
 * convertHDR10pMeta() :
 *  description :
 *      The functions takes in original dynamic metadata as an input and returns a modified one.
 *
 *  input parameters:
 *
 *      (1) in_metadata : current metadata
 *
 *      (2) in_len :  length of the in metadata for the purpose of verifying the size
 *
 *      (3) targetMaxLuminance : current target max luminance
 *
 *      (4) out_metadata : metadata after a conversion
 *
 *      (5) out_len : length of the out metadata for the purpose of verifying the size
 *
 *  returns:
 *
 *      0 on ok, else error
 *
 */
    virtual int convertHDR10pMeta(void __attribute__((unused)) *in_metadata, unsigned int __attribute__((unused)) in_len,
		    unsigned int __attribute__((unused)) targetMaxLuminance,
		    void __attribute__((unused)) *out_metadata, unsigned int __attribute__((unused)) out_len) { return HDRMETAIF_ERR_MAX; }

/*
 *
 * configHDR10Tonemap() :
 *  description:
 *      Max luminance of the source and the target are sent to the meta plugin through the interface
 *      for the purpose of computing the HDR10 tonemap.
 *
 *  input parameters:
 *
 *      (1) srcMasteringLuminance : source Mastering Luminance from static meta data
 *
 *      (2) targetMaxLuminance : current target Max Luminance
 *
 *  returns:
 *
 *      0 on ok, else error
 *
 */
    virtual int configHDR10Tonemap(
            unsigned int __attribute__((unused)) srcMasteringLuminance,
            unsigned int __attribute__((unused)) targetMaxLuminance
            ) { return HDRMETAIF_ERR_MAX; }
/*
 *
 * computeHDR10Tonemap() :
 *  description:
 *      The function yields a y value,
 *      each of which correspond to x values along the tonemap curve
 *
 *  input parameters:
 *
 *      (1) inX : a value between [0,1] * srcMasteringLuminance
 *
 *      (2) outY : a value between [0, targetMaxLuminance]
 *
 *  returns:
 *
 *      0 on ok, else error
 *
 */
    virtual int computeHDR10Tonemap(
            double __attribute__((unused)) inX,
            double __attribute__((unused)) *outY
            ) { return HDRMETAIF_ERR_MAX; }
/*
 *
 * getTargetLuminance() :
 *  description:
 *      the function modifies the target luminance in the middle of processing
 *
 *  input parameters:
 *
 *      (1) hdr capa : inner(0), outer(1)
 *
 *      (2) curTargetLuminance : current target luminance
 *
 *      (3) outTargetLuminance : target luminance modified by the function for the purpose of HDR video playback
 *
 *  returns:
 *
 *      0 on ok, else error
 *
 */
    virtual int getTargetLuminance(
            int __attribute__((unused)) hdr_capa,
            int __attribute__((unused)) curTargetLuminance,
            int __attribute__((unused)) *outTargetLuminance
            ) { return HDRMETAIF_ERR_MAX; }
/*
 *
 * setLogLevel() :
 *
 *  input parameters:
 *
 *      (1) log_level : value that signifies the level of log depth
 *
 *  returns:
 *
 *      void
 *
 */
    virtual void setLogLevel(int __attribute__((unused)) log_level) {}
};

#endif /* __HDR10P_META_INTERFACE_H__ */
