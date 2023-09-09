#ifndef __LIBHDR_MAIN_H__
#define __LIBHDR_MAIN_H__
#include <hardware/exynos/hdrInterface.h>
#include <hdrMetaInterface.h>
#include "hdrContext.h"
#include "hdrHwInfo.h"
#include "wcgCoef.h"
#include "hdr10Coef.h"
#include "hdr10pCoef.h"
#include "hlgCoef.h"
#include "hdrTuneCoef.h"
#include "extraCoef.h"

class hdrImplementation: public hdrInterface {
private:
    class hdrHwInfo hwInfo;
    class wcgCoef wcgInfo;
    class hdr10Coef hdr10Info;
    class hdr10pCoef hdr10pInfo;
    class hlgCoef hlgInfo;
    class hdrTuneCoef hdrTuneInfo;
    class extraCoef extraInfo;
    struct hdrContext Ctx;
public:
    hdrImplementation();
    ~hdrImplementation();
    int getHdrCoefSize(enum HdrHwId __attribute__((unused)) hw_id);

    int setTargetInfo(struct HdrTargetInfo __attribute__((unused)) *tInfo);
    void setHDRlayer(bool __attribute__((unused)) hasHdr);

    int initHdrCoefBuildup(enum HdrHwId __attribute__((unused)) hw_id);
    bool needHdrProcessing(struct HdrLayerInfo __attribute__((unused)) *lInfo);

    int setLayerInfo(int __attribute__((unused)) layer_index, struct HdrLayerInfo __attribute__((unused)) *lInfo);
    int getHdrCoefData(enum HdrHwId __attribute__((unused)) hw_id, int __attribute__((unused)) layer_index,
                struct hdrCoefParcel __attribute__((unused)) *parcel);

    void setLogLevel(int __attribute__((unused)) log_level);
};

#endif
