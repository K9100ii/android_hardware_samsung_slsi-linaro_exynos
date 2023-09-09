#include <hdrContext.h>

bool _HdrLayerInfo_::compare_matrix(float (*mat1)[4], float (*mat2)[4])
{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            if (mat1[i][j] != mat2[i][j])
                return false;
    return true;
}

void _HdrLayerInfo_::refineTransfer(int &ids) {
    int transfer = ids & HAL_DATASPACE_TRANSFER_MASK;
    int result = ids;
    switch (transfer) {
        case HAL_DATASPACE_TRANSFER_SRGB:
        case HAL_DATASPACE_TRANSFER_LINEAR:
        case HAL_DATASPACE_TRANSFER_ST2084:
        case HAL_DATASPACE_TRANSFER_HLG:
            break;
        case HAL_DATASPACE_TRANSFER_SMPTE_170M:
        case HAL_DATASPACE_TRANSFER_GAMMA2_2:
        case HAL_DATASPACE_TRANSFER_GAMMA2_6:
        case HAL_DATASPACE_TRANSFER_GAMMA2_8:
            if (ctx->OS_Version > 12)
                break;
            [[fallthrough]];
        default:
            result = result & ~(HAL_DATASPACE_TRANSFER_MASK);
            ids = (result | HAL_DATASPACE_TRANSFER_SRGB);
            break;
    }
}

void _HdrLayerInfo_::printDynamicMeta(ExynosHdrDynamicInfo_t *dyn_meta, std::string opt)
{
    int i;

    ALOGD("%s%s:+", __func__, opt.c_str());
    ALOGD("valid : %u",			dyn_meta->valid);
    ALOGD("country_code : %u", 		dyn_meta->data.country_code);
    ALOGD("provider_code : %u", 		dyn_meta->data.provider_code);
    ALOGD("provider_oriented_code : %u", 	dyn_meta->data.provider_oriented_code);
    ALOGD("application_identifier : %u", 	dyn_meta->data.application_identifier);
    ALOGD("application_version : %u",		dyn_meta->data.application_version);
    ALOGD("display_max_lum : %u", 		dyn_meta->data.display_maximum_luminance);
    ALOGD("maxscl : [%u %u %u]", 		dyn_meta->data.maxscl[0], dyn_meta->data.maxscl[1], dyn_meta->data.maxscl[2]);
    ALOGD("num_maxrgb_percentiles : %u", 	dyn_meta->data.num_maxrgb_percentiles);
    for (i = 0; i < 15; i+=5) {
        ALOGD("maxrgb_percentages[%4d~%4d] : %5d %5d %5d %5d %5d", i, i + 4,
                dyn_meta->data.maxrgb_percentages[i + 0], dyn_meta->data.maxrgb_percentages[i + 1],
                dyn_meta->data.maxrgb_percentages[i + 2], dyn_meta->data.maxrgb_percentages[i + 3],
                dyn_meta->data.maxrgb_percentages[i + 4]);
    }
    for (i = 0; i < 15; i+=5) {
        ALOGD("maxrgb_percentiles[%4d~%4d] : %5d %5d %5d %5d %5d", i, i + 4,
                dyn_meta->data.maxrgb_percentiles[i + 0], dyn_meta->data.maxrgb_percentiles[i + 1],
                dyn_meta->data.maxrgb_percentiles[i + 2], dyn_meta->data.maxrgb_percentiles[i + 3],
                dyn_meta->data.maxrgb_percentiles[i + 4]);
    }
    ALOGD("tone_mapping_flag : %d", dyn_meta->data.tone_mapping.tone_mapping_flag);
    ALOGD("knee point x : %d", dyn_meta->data.tone_mapping.knee_point_x);
    ALOGD("knee point y : %d", dyn_meta->data.tone_mapping.knee_point_y);
    ALOGD("num bezier curve anchors : %d", dyn_meta->data.tone_mapping.num_bezier_curve_anchors);
    for (i = 0; i < 15; i+=5) {
        ALOGD("bezier_curve_anchors[%4d~%4d] : %5d %5d %5d %5d %5d", i, i + 4,
                dyn_meta->data.tone_mapping.bezier_curve_anchors[i + 0], dyn_meta->data.tone_mapping.bezier_curve_anchors[i + 1],
                dyn_meta->data.tone_mapping.bezier_curve_anchors[i + 2], dyn_meta->data.tone_mapping.bezier_curve_anchors[i + 3],
                dyn_meta->data.tone_mapping.bezier_curve_anchors[i + 4]);
    }
    ALOGD("%s%s:-", __func__, opt.c_str());
}

int _HdrLayerInfo_::validateDynamicMeta(void)
{
    int i;
    ExynosHdrDynamicInfo_t *dyn_meta = &this->dyn_meta;
    for (i = 1; i < dyn_meta->data.num_maxrgb_percentiles; i++) {
        if (!dyn_meta->data.maxrgb_percentages[i]) {
            ALOGD("invalid dynamic meta: num_maxrgb_percentages(%d) exceed the array(idx:%d, err:%d)",
                    dyn_meta->data.num_maxrgb_percentiles, i, dyn_meta->data.maxrgb_percentages[i]);
            return 0;
        }
    }
    if (dyn_meta->data.tone_mapping.tone_mapping_flag)
        for (i = 1; i < dyn_meta->data.tone_mapping.num_bezier_curve_anchors; i++) {
            if (!dyn_meta->data.tone_mapping.bezier_curve_anchors[i]) {
                ALOGD("invalid dynamic meta: num_bezier_curve_anchors(%d) exceed the array(idx:%d, err:%d)",
                        dyn_meta->data.tone_mapping.num_bezier_curve_anchors, i, dyn_meta->data.tone_mapping.bezier_curve_anchors[i]);
                return 0;
            }
        }
    return 0;
}

bool _HdrLayerInfo_::changed(struct HdrLayerInfo *lInfo) {
    int ds = lInfo->dataspace & (HAL_DATASPACE_STANDARD_MASK | HAL_DATASPACE_TRANSFER_MASK);
    int cur_ds = this->dataspace & (HAL_DATASPACE_STANDARD_MASK | HAL_DATASPACE_TRANSFER_MASK);

    if (log_level > 0) this->dump_changed(lInfo);

    if (this->target_changed == true)
        goto changed;

    if (this->active != true)
        goto changed;

    if (lInfo->bpc != this->bpc
            || lInfo->premult_alpha != this->premult_alpha
            || lInfo->bypass != this->bypass)
        goto changed;

    if (lInfo->tf_matrix != NULL) {
        if (this->has_tf_matrix == false)
            goto changed;
        else
            if (compare_matrix((float (*)[4])lInfo->tf_matrix, this->transform_matrix) == false)
                goto changed;
    } else {
        if (this->has_tf_matrix == true)
            goto changed;
    }

    if (ctx->getHdrType(lInfo) != layerHdrType::NONE)
        goto changed;

    if (ds != cur_ds)
        goto changed;

    if (lInfo->static_metadata)
        if (lInfo->static_len == sizeof(ExynosHdrStaticInfo)) {
            if (this->mastering_luminance != (unsigned int)(((ExynosHdrStaticInfo*)lInfo->static_metadata)->sType1.mMaxDisplayLuminance / 10000))
                goto changed;
            if (this->max_cll != (unsigned int)(((ExynosHdrStaticInfo*)lInfo->static_metadata)->sType1.mMaxContentLightLevel))
                goto changed;
        }

    if (lInfo->dynamic_metadata)
        if (lInfo->dynamic_len == sizeof(ExynosHdrDynamicInfo))
            goto changed;

    this->transfer();
    return false;
changed:
    this->save(layer_index, lInfo);
    return true;
}

void _HdrLayerInfo_::save(int layer_index, struct HdrLayerInfo *lInfo) {
    this->layer_index = layer_index;
    this->dataspace = lInfo->dataspace;
    this->premult_alpha = lInfo->premult_alpha;
    this->bpc = lInfo->bpc;
    this->bypass = lInfo->bypass;
    lInfo->tf_matrix != NULL ? this->has_tf_matrix = true :  this->has_tf_matrix = false;
    this->mastering_luminance = 0;
    this->max_cll = 0;
    this->setTargetLuminance(lInfo);
    if (lInfo->static_metadata)
        if (lInfo->static_len == sizeof(ExynosHdrStaticInfo)) {
            this->mastering_luminance = (unsigned int)(((ExynosHdrStaticInfo*)lInfo->static_metadata)->sType1.mMaxDisplayLuminance / 10000);
            this->max_cll = (unsigned int)(((ExynosHdrStaticInfo*)lInfo->static_metadata)->sType1.mMaxContentLightLevel);
        }
    if (lInfo->dynamic_metadata && lInfo->dynamic_len == sizeof(ExynosHdrDynamicInfo)) {
        ExynosHdrDynamicInfo out_metadata;
        if (ctx->metaIf != nullptr && ctx->metaIf->convertHDR10pMeta(
                    (ExynosHdrDynamicInfo*)lInfo->dynamic_metadata,
                    sizeof(ExynosHdrDynamicInfo),
                    this->target_luminance,
                    &out_metadata,
                    sizeof(ExynosHdrDynamicInfo)) == HDRMETAIF_ERR_NO) {
            convertDynamicMeta(&this->dyn_meta, (ExynosHdrDynamicInfo*)&out_metadata);
            if (log_level > 0) printDynamicMeta(&this->dyn_meta, "-overrided");
        } else {
            convertDynamicMeta(&this->dyn_meta, (ExynosHdrDynamicInfo*)lInfo->dynamic_metadata);
        }
    }
    this->shall.clear();
    this->need.clear();
    for (int i = 0; i < HDR_HW_MAX; ++i)
        this->group[i].clear();
    this->target_changed = false;
    this->layer_changed = true;
}

void _HdrLayerInfo_::setTargetLuminance(struct HdrLayerInfo *lInfo) {
    enum HdrTargetLuminanceType type = HdrTargetLuminanceType::DEFAULT;

    if (ctx->target_luminance[HdrTargetLuminanceType::COMMAND] > 0)
        type = HdrTargetLuminanceType::COMMAND;
    else if (ctx->target_luminance[HdrTargetLuminanceType::INTERFACE] > 0 &&
            ctx->getHdrType(lInfo) != layerHdrType::NONE &&
            (lInfo->dataspace & HAL_DATASPACE_STANDARD_MASK) == HAL_DATASPACE_STANDARD_BT2020)
        type = HdrTargetLuminanceType::INTERFACE;

    this->target_luminance = ctx->target_luminance[type];
}

void _HdrLayerInfo_::transfer(void) {
    int i = 0;
    for (auto iter = this->shall.begin(); iter != this->shall.end(); iter++) {
        need.push_back(*iter);
        i++;
    }
    if (i > 0) this->layer_changed = true;
    else this->layer_changed = false;
    this->shall.clear();
}

void _HdrLayerInfo_::setActive (void) {
    this->active = true;
}

void _HdrLayerInfo_::setInActive (void) {
    this->active = false;
}

int _HdrLayerInfo_::getHdrCoefData(void *hdrCoef) {
    char *dat = (char*)hdrCoef;
    int wr_offset = 0;
    struct hdr_coef_header header_g;
    unsigned int total_bytesize = sizeof(struct hdr_coef_header);
    unsigned int _shall = 0, _need = 0;

    clean_duplicate();
    for (auto iter = this->shall.begin(); iter != this->shall.end(); iter++) {
        _shall++;
        total_bytesize += (*iter)->size();
    }
    for (auto iter = need.begin(); iter != need.end(); iter++) {
        _need++;
        total_bytesize += (*iter)->size();
    }
    header_g.init(total_bytesize, this->layer_index,
            log_level,
            (unsigned int)this->premult_alpha,
            (unsigned int)this->active,
            _shall, _need);

    if (this->layer_changed) {
        /* copy layer info to fd */
        memcpy(dat + wr_offset, &header_g, sizeof(header_g));
        wr_offset += sizeof(header_g);

        for (auto iter = shall.begin(); iter != shall.end(); iter++) {
            (*iter)->serialize(dat, wr_offset, (*iter)->size());
            wr_offset+=(*iter)->size();
        }

        for (auto iter = need.begin(); iter != need.end(); iter++) {
            (*iter)->serialize(dat, wr_offset, (*iter)->size());
            wr_offset+=(*iter)->size();
        }
    }

    if (log_level > 1) {
        header_g.dump(0);
        for (auto iter = shall.begin(); iter != shall.end(); iter++)
            (*iter)->dump(1);
        for (auto iter = need.begin(); iter != need.end(); iter++)
            (*iter)->dump(1);
    }
    return HDR_ERR_NO;
}

int _HdrLayerInfo_::getCoefSize (void) {
    int size = sizeof(struct hdr_coef_header);
    for (auto iter = shall.begin(); iter != shall.end(); ++iter)
        size += (sizeof(hdr_lut_header) + (sizeof(int) * (*iter)->header.length));
    for (auto iter = need.begin(); iter != need.end(); ++iter)
        size += (sizeof(hdr_lut_header) + (sizeof(int) * (*iter)->header.length));
    return size;
}

void _HdrLayerInfo_::clean_duplicate (void) {
    for (auto iter = shall.end(); iter != shall.begin();) {
        --iter;
        for (auto nIter = iter; nIter != shall.begin();) {
            --nIter;
            if ((*iter)->header.byte_offset == (*nIter)->header.byte_offset &&
                    (*iter)->header.length == (*nIter)->header.length &&
                    (*iter)->group_id == (*nIter)->group_id) {
                if (log_level > 1) {
                    ALOGD("clean dup [0x%x] 0x%08x,0x%08x",
                            (*nIter)->header.byte_offset,
                            ((unsigned int*)(*nIter)->data)[0],
                            ((unsigned int*)(*nIter)->data)[1]);
                }
                nIter = shall.erase(nIter);
            }
        }
    }
}

void _HdrLayerInfo_::dump_changed(struct HdrLayerInfo *lInfo) {
    ALOGD(" > layer_index : %d", this->layer_index);
    ALOGD("-----[prev]-----");
    ALOGD("dataspace : %d", this->dataspace);
    ALOGD("premultiplied alpha(%d), bpc(%d)", (int)this->premult_alpha, (int)this->bpc);
    ALOGD("mastering luminance(%u)", this->mastering_luminance);
    ALOGD("max cll(%u)", this->max_cll);
    if (lInfo->tf_matrix != NULL)
        ALOGD("tf_matrix(0x%lx)", (unsigned long)this->transform_matrix);
    ALOGD("bypass(%d)", (int)this->bypass);
    ALOGD("-----[cur]-----");
    ALOGD("dataspace : %d", lInfo->dataspace);
    if (lInfo->static_metadata != NULL) {
        ALOGD("static_meta(0x%lx) length(%d)", (unsigned long)lInfo->static_metadata, lInfo->static_len);
        if (lInfo->static_len == sizeof(ExynosHdrStaticInfo)) {
            ALOGD("mastering luminance(%d)", ((ExynosHdrStaticInfo*)lInfo->static_metadata)->sType1.mMaxDisplayLuminance / 10000);
            ALOGD("max cll (%d)", ((ExynosHdrStaticInfo*)lInfo->static_metadata)->sType1.mMaxContentLightLevel);
        }
    }
    if (lInfo->dynamic_metadata != NULL) {
        ALOGD("dynamic_meta(0x%lx) length(%d)", (unsigned long)lInfo->dynamic_metadata, lInfo->dynamic_len);
        ExynosHdrDynamicInfo_t dyn_meta;
        convertDynamicMeta(&dyn_meta, (ExynosHdrDynamicInfo*)lInfo->dynamic_metadata);
        printDynamicMeta(&dyn_meta);
    }
    ALOGD("premultiplied alpha(%d), bpc(%d)", (int)lInfo->premult_alpha, (int)lInfo->bpc);
    if (lInfo->tf_matrix != NULL)
        ALOGD("tf_matrix(0x%lx)", (unsigned long)lInfo->tf_matrix);
    ALOGD("bypass(%d)\n", (int)lInfo->bypass);
}

void _HdrLayerInfo_::setLogLevel(int log_level) {
    this->log_level = log_level;
}

void hdrContext::init (class hdrHwInfo *hwInfo) {
    Target.dataspace = -1;
    target_name = this->getTargetName();
    std::vector<struct supportedHdrHw> *hwList = hwInfo->getListHdrHw();
    this->OS_Version = this->getOSVersion();
    metaIf = hdrMetaInterface::createInstance();
    moduleSpecifiers.init(hwInfo);
    for (auto iter = hwList->begin(); iter != hwList->end(); iter++) {
        Layers[iter->id].clear();
        Layers[iter->id].resize(iter->IHw->getLayerNum());
        for (int i = 0; i < iter->IHw->getLayerNum(); i++) {
            Layers[iter->id][i].layer_index = i;
            Layers[iter->id][i].ctx = this;
        }
    }
    state = hdrPerFrameState::OBJ_READY;
}

std::string hdrContext::getTargetName(void)
{
    std::string TARGET_NAME_SYSFS_NODE = "/sys/class/dqe/dqe/xml";
    int LEN = 128;
    int target_name_fd;
    char target_name[LEN];
    int rd_len;
    std::string tName = "\0";
    target_name[0] = '\0';
    target_name_fd = open(TARGET_NAME_SYSFS_NODE.c_str(), O_RDONLY);
    if (target_name_fd >= 0) {
        rd_len = read(target_name_fd, target_name, LEN);
        if (rd_len > 0 && rd_len < LEN) {
            tName = "_";
            target_name[rd_len] = '\0';
            tName += target_name;
            tName.pop_back();
            ALOGD("target name(%s) confirmed", tName.c_str());
        } else
            ALOGD("no target name is found(rd_len:%d)", rd_len);
        close(target_name_fd);
    } else
        ALOGD("no target name is found(fd(%d))", target_name_fd);
    return tName;
}

int hdrContext::getOSVersion(void)
{
    char PROP_OS_VERSION[PROPERTY_VALUE_MAX] = "ro.build.version.release";
    char str_value[PROPERTY_VALUE_MAX] = {0};
    property_get(PROP_OS_VERSION, str_value, "0");
    int OS_Version = stoi(std::string(str_value));
    ALOGD("OS VERSION : %d", OS_Version);
    return OS_Version;
}
void hdrContext::setHwId(int hw_id) {
    this->hw_id = hw_id;
    if (log_level > 0) dumpTargetInfo(&Target);
}

void hdrContext::setHdrLayer(bool hasHdr) {
    this->hasHdrLayer = hasHdr;
    this->setTargetLuminance(HdrTargetLuminanceType::INTERFACE);
}

void hdrContext::setTargetInfo(struct HdrTargetInfo *Target) {
    memcpy(&this->Target, Target, sizeof(struct HdrTargetInfo));
    this->setTargetLuminance(HdrTargetLuminanceType::DEFAULT, this->Target.max_luminance);

    for (int i = 0; i < HDR_HW_MAX; i++)
        for (auto iter = Layers[i].begin(); iter != Layers[i].end(); iter++)
            iter->target_changed = true;
}

void hdrContext::dumpTargetInfo(struct HdrTargetInfo *Target) {
    ALOGD("[Target Info]");
    ALOGD("dataspace : %d", Target->dataspace);
    ALOGD("min lum : %u", Target->min_luminance);
    ALOGD("max lum : %u", Target->max_luminance);
    ALOGD("bpc : %d", (int)Target->bpc);
    ALOGD("hdr_capa : %d", (int)Target->hdr_capa);
}

bool hdrContext::needHdrProcessing(struct HdrLayerInfo *lInfo) {
    if (Target.dataspace >= 0 && lInfo->dataspace >= 0) {
        int ds_target = Target.dataspace & (HAL_DATASPACE_STANDARD_MASK | HAL_DATASPACE_TRANSFER_MASK);
        int ds_source = lInfo->dataspace & (HAL_DATASPACE_STANDARD_MASK | HAL_DATASPACE_TRANSFER_MASK);
        if (ds_target != ds_source)
            return true;

        if (this->getHdrType(lInfo) != layerHdrType::NONE)
            return true;
    }
    return false;
}

void hdrContext::setTuneMode(bool enable) {
    tune_mode = enable;
    tune_reload = enable;
}

void hdrContext::setSourceLuminance(unsigned int luminance) {
    this->source_luminance = luminance;
}

void hdrContext::setTargetLuminance(enum HdrTargetLuminanceType type,
                                            unsigned int luminance) {
    switch (type) {
        case HdrTargetLuminanceType::DEFAULT :
            this->target_luminance[type] = luminance;
            if (this->target_luminance[type] <= 0)
                this->target_luminance[type] = 500;
            break;
        case HdrTargetLuminanceType::COMMAND :
            this->target_luminance[type] = luminance;
            break;
        case HdrTargetLuminanceType::INTERFACE :
            int tLumForHdr;
            if (this->hasHdrLayer == true &&
                metaIf != nullptr &&
                metaIf->getTargetLuminance(
                        (int)Target.hdr_capa,
                        this->target_luminance[HdrTargetLuminanceType::DEFAULT],
                        &tLumForHdr) == HDRMETAIF_ERR_NO)
                this->target_luminance[type] = tLumForHdr;
            else
                this->target_luminance[type] = 0;
            break;
        default:
            break;
    }
}

enum layerHdrType hdrContext::getHdrType(struct HdrLayerInfo *lInfo) {
    enum layerHdrType ret = layerHdrType::NONE;
    unsigned int tf = lInfo->dataspace & HAL_DATASPACE_TRANSFER_MASK;

    if (tf == HAL_DATASPACE_TRANSFER_ST2084) {
        if (lInfo->dynamic_metadata != NULL)
            ret = layerHdrType::HDR10P;
        else
            ret = layerHdrType::HDR10;
    } else if (tf == HAL_DATASPACE_TRANSFER_HLG)
        ret = layerHdrType::HLG;

    return ret;
}

void hdrContext::setLogLevel(int log_level) {
    this->log_level = log_level;
    if (metaIf != nullptr)
        metaIf->setLogLevel(log_level);
    for (int id = 0; id < HDR_HW_MAX; id++)
        for (auto iter = Layers[id].begin(); iter != Layers[id].end(); iter++)
            iter->setLogLevel(log_level);
}
