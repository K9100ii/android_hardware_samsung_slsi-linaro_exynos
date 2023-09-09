#include "hdrContext.h"
#include <unistd.h>
#include "extraCoef.h"

using namespace std;

void extraCoef::init(hdrHwInfo *hwInfo, struct hdrContext __attribute__((unused)) * ctx)
{
    this->hwInfo = hwInfo;
    //this->dump();
}

int extraCoef::coefBuildup(int layer_index, struct hdrContext *ctx)
{
    int ret = HDR_ERR_NO;
    int hw_id = ctx->hw_id;
    auto layer = &ctx->Layers[hw_id][layer_index];

    if (layer->log_level > 0) {
        if (layer->getCoefSize() > hwInfo->getIf(hw_id)->getHdrCoefSize()) {
            ALOGE("size overflow : %d > %d", layer->getCoefSize(),
                                        hwInfo->getIf(hw_id)->getHdrCoefSize());
        }
    }
    return ret;
}

