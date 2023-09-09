#include "libhdr.h"

hdrImplementation::hdrImplementation()
{
    hwInfo.init();
    Ctx.init(&hwInfo);

    wcgInfo.init(&hwInfo, &Ctx);
    hdr10Info.init(&Ctx);
    hdr10pInfo.init(&Ctx);
    hlgInfo.init(&Ctx);
    hdrTuneInfo.init(&hwInfo, &Ctx);
    extraInfo.init(&hwInfo, &Ctx);
}

hdrImplementation::~hdrImplementation()
{
}

int hdrImplementation::setTargetInfo(struct HdrTargetInfo __attribute__((unused)) *tInfo)
{
    LIBHDR_LOGD(Ctx.log_level, "%s +", __func__);
    if (Ctx.state < hdrPerFrameState::OBJ_READY)
        return HDR_ERR_INVAL;
    Ctx.setTargetInfo(tInfo);

    if (Ctx.state < hdrPerFrameState::TARGET_READY) {
        hdr10Info.parse(&hwInfo, &Ctx);
        hdr10pInfo.parse(&hwInfo, &Ctx);
        hlgInfo.parse(&hwInfo, &Ctx);
    }

    Ctx.state = hdrPerFrameState::TARGET_READY;
    LIBHDR_LOGD(Ctx.log_level, "%s -", __func__);
    return HDR_ERR_NO;
}

int hdrImplementation::initHdrCoefBuildup(enum HdrHwId __attribute__((unused)) hw_id)
{
    LIBHDR_LOGD(Ctx.log_level, "%s +", __func__);
    if (Ctx.state < hdrPerFrameState::TARGET_READY)
        return HDR_ERR_INVAL;
    Ctx.state = hdrPerFrameState::BUILD_UP_READY;
    Ctx.setHwId(hw_id);
    auto layer_info             = &Ctx.Layers[hw_id];
    for (auto iter = layer_info->begin(); iter != layer_info->end(); iter++)
        iter->state = hdrPerLayerState::INIT;
    LIBHDR_LOGD(Ctx.log_level, "%s -", __func__);
    return HDR_ERR_NO;
}

int hdrImplementation::getHdrCoefSize(enum HdrHwId __attribute__((unused)) hw_id)
{
    LIBHDR_LOGD(Ctx.log_level, "%s +", __func__);
    LIBHDR_LOGD(Ctx.log_level, "%s -", __func__);
    return hwInfo.getIf(hw_id)->getHdrCoefSize();
}

void hdrImplementation::setHDRlayer(bool __attribute__((unused)) hasHdr)
{
    LIBHDR_LOGD(Ctx.log_level, "%s +", __func__);
    Ctx.setHdrLayer(hasHdr);
    LIBHDR_LOGD(Ctx.log_level, "%s -", __func__);
}

bool hdrImplementation::needHdrProcessing(struct HdrLayerInfo __attribute__((unused)) *lInfo)
{
    LIBHDR_LOGD(Ctx.log_level, "%s +", __func__);
    LIBHDR_LOGD(Ctx.log_level, "%s -", __func__);
    return Ctx.needHdrProcessing(lInfo);
}

int hdrImplementation::setLayerInfo(int __attribute__((unused)) layer_index,
                        struct HdrLayerInfo __attribute__((unused)) *lInfo)
{
    LIBHDR_LOGD(Ctx.log_level, "%s +", __func__);
    if (Ctx.state < hdrPerFrameState::BUILD_UP_READY)
        return HDR_ERR_INVAL;

    auto layer_info             = &Ctx.Layers[Ctx.hw_id][layer_index];
    enum layerHdrType hdr_type  = Ctx.getHdrType(lInfo);
    int ret                     = HDR_ERR_NO;

    layer_info->refineTransfer(lInfo->dataspace);
    if (layer_info->changed(lInfo) == false) {
        layer_info->state = hdrPerLayerState::BUILD_UP_READY;
        return ret;
    }

    if (hdr_type != layerHdrType::NONE && Ctx.tune_mode == true) {
        ret = hdrTuneInfo.coefBuildup(layer_index, &Ctx);
    } else {
        // set base SFRs (EOTF/GM/OETF) from wcg module
        ret = wcgInfo.coefBuildup(layer_index, &Ctx);
        switch (hdr_type) {
            case layerHdrType::HDR10:
                ret = hdr10Info.coefBuildup(layer_index, &Ctx);
                break;
            case layerHdrType::HDR10P:
                ret = hdr10pInfo.coefBuildup(layer_index, &Ctx);
                break;
            case layerHdrType::HLG:
                ret = hlgInfo.coefBuildup(layer_index, &Ctx);
                break;
            default:
                break;
        }
        extraInfo.coefBuildup(layer_index, &Ctx);
    }

    if (ret == HDR_ERR_NO)
        layer_info->state = hdrPerLayerState::BUILD_UP_READY;
    else
        layer_info->state = hdrPerLayerState::INIT;
    LIBHDR_LOGD(Ctx.log_level, "%s -", __func__);
    return ret;
}

int hdrImplementation::getHdrCoefData(enum HdrHwId __attribute__((unused)) hw_id,
                            int __attribute__((unused)) layer_index,
                            struct hdrCoefParcel __attribute__((unused)) *parcel)
{
    LIBHDR_LOGD(Ctx.log_level, "%s +", __func__);
    if (Ctx.state < hdrPerFrameState::BUILD_UP_READY)
        return HDR_ERR_INVAL;

    auto layer_info             = &Ctx.Layers[hw_id][layer_index];
    if (layer_info->state != hdrPerLayerState::BUILD_UP_READY)
        return HDR_ERR_INVAL;
    LIBHDR_LOGD(Ctx.log_level, "%s -", __func__);
    return Ctx.Layers[hw_id][layer_index].getHdrCoefData(parcel->hdrCoef);
}

void hdrImplementation::setLogLevel(int __attribute__((unused)) log_level)
{
    LIBHDR_LOGD(Ctx.log_level, "%s +", __func__);
    if (log_level == 0) { /* reset */
        Ctx.setTuneMode(false);
        Ctx.setTargetLuminance(HdrTargetLuminanceType::COMMAND, 0);
        Ctx.setSourceLuminance(0);
    } else if (log_level < 0 || log_level > 100) { /* debug mode */
        switch (log_level) {
            case -1: /* tune mode on */
                Ctx.setTuneMode(true);
                break;
            default: /* override taget luminance */
                if (log_level < 0)
                    Ctx.setTargetLuminance(HdrTargetLuminanceType::COMMAND, abs(log_level));
                else
                    Ctx.setSourceLuminance(log_level);
                break;
        }
        log_level = Ctx.log_level; // restore
    }
    Ctx.setLogLevel(log_level);
    LIBHDR_LOGD(Ctx.log_level, "%s -", __func__);
}

hdrInterface *hdrInterface::createInstance(void) {
    return new hdrImplementation();
}

