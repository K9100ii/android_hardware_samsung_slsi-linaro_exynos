#ifndef __HDR_CONTEXT_H__
#define __HDR_CONTEXT_H__
#include <hardware/exynos/hdrInterface.h>
#include <hdrMetaInterface.h>
#include <system/graphics.h>
#ifndef HDR_TEST
#include <VendorVideoAPI.h>
#else
#include <VendorVideoAPI_hdrTest.h>
#endif
#include <deque>
#include <vector>
#include <list>
#include <cutils/properties.h>
#include "libhdr_parcel_header.h"
#include "hdrHwInfo.h"
#include "unistd.h"
#include "dynamic_info_legacy.h"

#include <fcntl.h>
#include "hdrModuleSpecifiers.h"

namespace hdrPerLayerState {
enum layerState {
    INIT = 0,
    BUILD_UP_READY,
};
}
enum layerHdrType {
    NONE = 0,
    HDR10 = 1,
    HDR10P = 2,
    HLG = 3,
};

struct _HdrLayerInfo_ {
    hdrPerLayerState::layerState state = hdrPerLayerState::INIT;
    struct hdrContext *ctx = nullptr;
    int active = false;
    bool layer_changed = false;
    int layer_index = -1;
    unsigned int target_luminance = 0;
    /* data space context */
    int dataspace;
    bool premult_alpha = false;
    enum HdrBpc bpc;
    bool bypass = false;
    bool target_changed = false;

    /* static meta */
    unsigned int mastering_luminance = 0;
    unsigned int max_cll = 0;
    /* dynamic meta */
    ExynosHdrDynamicInfo_t dyn_meta;
    /* final src lum by static/dynamic meta combination */
    unsigned int source_luminance = 0;

    /* extra fuction context */
    bool has_tf_matrix = false;
    float transform_matrix[4][4];

    int log_level = 0;

    /* shall/need for packing */
    std::list<struct hdr_dat_node*> shall;
    std::list<struct hdr_dat_node*> need;
    std::unordered_map<int, struct hdr_dat_node> group[HDR_HW_MAX];

    bool compare_matrix(float (*mat1)[4], float (*mat2)[4]);
    void refineTransfer(int &ids);
    void printDynamicMeta(ExynosHdrDynamicInfo_t *dyn_meta, std::string opt = "");
    int validateDynamicMeta(void);
    bool changed(struct HdrLayerInfo *lInfo);
    void save(int layer_index, struct HdrLayerInfo *lInfo);
    void setTargetLuminance(struct HdrLayerInfo *lInfo);
    void transfer(void);
    void setActive (void);
    void setInActive (void);
    int getHdrCoefData(void *hdrCoef);
    int getCoefSize (void);
    void clean_duplicate (void);
    void dump_changed(struct HdrLayerInfo *lInfo);
    void setLogLevel(int log_level);
};

namespace hdrPerFrameState {
enum perFrameState {
    INIT = 0,
    OBJ_READY,
    TARGET_READY,
    BUILD_UP_READY,
};
}

enum HdrTargetLuminanceType {
    DEFAULT = 0,
    COMMAND,
    INTERFACE,
    MAX,
};

struct hdrContext {
    hdrPerFrameState::perFrameState state = hdrPerFrameState::INIT;
    int OS_Version = 12;
    int log_level = 0;
    bool tune_mode = false;
    bool tune_reload = false;
    int hw_id = HDR_HW_MAX;
    bool hasHdrLayer = false;
    std::deque<struct _HdrLayerInfo_> Layers[HDR_HW_MAX];
    struct HdrTargetInfo Target;
    std::string target_name;
    unsigned int source_luminance = 0;
    unsigned int target_luminance[HdrTargetLuminanceType::MAX] = {0};
    class hdrModuleSpecifiers moduleSpecifiers;
    class hdrMetaInterface* metaIf;

    void init (class hdrHwInfo *hwInfo);
    std::string getTargetName(void);
    int getOSVersion(void);
    void setHwId(int hw_id);
    void setHdrLayer(bool hasHdr);
    void setTargetInfo(struct HdrTargetInfo *Target);
    void dumpTargetInfo(struct HdrTargetInfo *Target);
    bool needHdrProcessing(struct HdrLayerInfo *lInfo);
    void setTuneMode(bool enable);
    void setSourceLuminance(unsigned int luminance = 0);
    void setTargetLuminance(enum HdrTargetLuminanceType type,
                                            unsigned int luminance = 0);
    enum layerHdrType getHdrType(struct HdrLayerInfo *lInfo);
    void setLogLevel(int log_level);
};

#endif
