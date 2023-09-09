#ifndef __EXTRA_COEF_H__
#define __EXTRA_COEF_H__
#include <system/graphics.h>
#include "libhdr_parcel_header.h"
#include "hdrUtil.h"
#include "hdrHwInfo.h"
#include "hdrModuleSpecifiers.h"
#include <algorithm>

class extraCoef {
private:
    hdrHwInfo *hwInfo = NULL;
  public:
    void init(hdrHwInfo *hwInfo,
                struct hdrContext __attribute__((unused)) *ctx);
    int coefBuildup(int layer_index,
                struct hdrContext *ctx);
};

#endif
