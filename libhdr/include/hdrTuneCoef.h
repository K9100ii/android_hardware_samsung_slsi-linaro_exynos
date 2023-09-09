#ifndef __HDR_TUNE_COEF_H__
#define __HDR_TUNE_COEF_H__
#include <system/graphics.h>
#include "libhdr_parcel_header.h"
#include "hdrUtil.h"
#include "hdrHwInfo.h"

struct hdrTuneNode {
    std::vector<struct hdr_dat_node> coef_packed;
};

class hdrTuneCoef {
private:
    const char* __attribute__((unused)) TAG = "hdrTuneCoef";
    struct hdrTuneModule {
        std::vector<struct hdrTuneNode>        hdrTuneNodeTable;
    };
    hdrHwInfo *hwInfo = NULL;
    std::string tune_filename = "/data/vendor/hdr/hdrTuneLut";
    std::string vendor_filename = "/vendor/etc/dqe/hdrTuneLut";
    std::unordered_map<int, struct hdrTuneModule> layerToHdrTuneMod;
    void parse(std::vector<struct supportedHdrHw> *list, struct hdrContext *ctx);
    void __parse__(int hw_id, struct hdrContext *ctx);
    void parse_hdrTuneMods(
        int hw_id,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module);
    void parse_hdrTuneMod(
        int hw_id,
        int module_id,
        struct hdrTuneModule &hdrTune_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module);
    void parse_hdrTuneNode(
        int hw_id,
        int module_id,
        struct hdrTuneNode __attribute__((unused)) &hdrTune_node,
        xmlDocPtr __attribute__((unused)) xml_doc,
        xmlNodePtr xml_hdrTunenode);
public:
    void init(hdrHwInfo *hwInfo,
                struct hdrContext *ctx);
    int coefBuildup(int layer_index,
                struct hdrContext *out);
};

#endif
