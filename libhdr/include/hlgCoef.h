#ifndef __HLG_COEF_H__
#define __HLG_COEF_H__
#include <system/graphics.h>
#include "libhdr_parcel_header.h"
#include "hdrUtil.h"
#include "hdrHwInfo.h"
#include <algorithm>

struct hlgNode {
    std::vector<struct hdr_dat_node> coef_packed;
};

class hlgCoef {
private:
    const char* __attribute__((unused)) TAG = "hlgCoef";
    struct hlgModule {
        std::vector<struct hlgNode>        hlgNodeTable;
    };
    hdrHwInfo *hwInfo = NULL;
    struct hdrInfo hlginfo;
    std::unordered_map<std::string, int> capStrMap;
    std::unordered_map<std::string, int> stStrMap;
    std::unordered_map<std::string, int> tfStrMap;
    std::unordered_map<int, struct hlgModule> layerToHlgMod;
    void parse(std::vector<struct supportedHdrHw> *list, struct hdrContext *ctx);
    void __parse__(int hw_id, struct hdrContext *ctx);
    void parse_hlgMods(
        int hw_id,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module);
    void parse_hlgMod(
        int hw_id,
        int module_id,
        struct hlgModule &hlg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module);
    void parse_hlgNode(
        int hw_id,
        int module_id,
        struct hlgNode __attribute__((unused)) &hlg_node,
        xmlDocPtr __attribute__((unused)) xml_doc,
        xmlNodePtr xml_hlgnode);
    void parse_hdrNodes(
            xmlDocPtr xml_doc,
            xmlNodePtr xml_nodes);
    void parse_hdrSubNodes(
            xmlDocPtr xml_doc,
            xmlNodePtr xml_subnodes);
    void tm_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct tonemapModuleSpecifier *tmModSpecifier);
    void eotf_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct eotfModuleSpecifier *eotfModSpecifier);
    unsigned int getSourceLuminance();
public:
    hlgCoef (void);
    void parse(hdrHwInfo *hwInfo,
                struct hdrContext *ctx);
    int coefBuildup(int layer_index,
                struct hdrContext *out);
    void init(struct hdrContext *ctx);
};

#endif
