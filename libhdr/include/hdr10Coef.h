#ifndef __HDR10_COEF_H__
#define __HDR10_COEF_H__
#include <system/graphics.h>
#include "libhdr_parcel_header.h"
#include "hdrUtil.h"
#include "hdrHwInfo.h"
#include <algorithm>

struct hdr10Node {
    int max_luminance;
    std::vector<struct hdr_dat_node> coef_packed;
};

class hdr10Coef {
private:
    const char* __attribute__((unused)) TAG = "hdr10Coef";
    struct hdr10Module {
        std::vector<struct hdr10Node>        hdr10NodeTable;
        void sort_ascending(void) {
            std::sort(hdr10NodeTable.begin(), hdr10NodeTable.end(),
                    [] (struct hdr10Node &op1, struct hdr10Node &op2) {
                        return op1.max_luminance < op2.max_luminance;
                    });
        }
    };
    hdrHwInfo *hwInfo = NULL;
    struct hdrInfo hdr10info;
    std::unordered_map<std::string, int> capStrMap;
    std::unordered_map<std::string, int> stStrMap;
    std::unordered_map<std::string, int> tfStrMap;
    std::unordered_map<int, struct hdr10Module> layerToHdr10Mod;
    void parse(std::vector<struct supportedHdrHw> *list, struct hdrContext *ctx);
    void __parse__(int hw_id, struct hdrContext *ctx);
    void parse_hdr10Mods(
        int hw_id,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module);
    void parse_hdr10Mod(
        int hw_id,
        int module_id,
        struct hdr10Module &hdr10_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module);
    void parse_hdr10Node(
        int hw_id,
        int module_id,
        struct hdr10Node __attribute__((unused)) &hdr10_node,
        xmlDocPtr __attribute__((unused)) xml_doc,
        xmlNodePtr xml_hdr10node);
    void parse_hdrNodes(
            xmlDocPtr xml_doc,
            xmlNodePtr xml_nodes);
    void parse_hdrSubNodes(
            xmlDocPtr xml_doc,
            xmlNodePtr xml_subnodes);
    void tm_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct hdrContext *ctx,
        struct tonemapModuleSpecifier *tmModSpecifier);
    void pq_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct pqModuleSpecifier *pqModSpecifier);
    void eotf_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct eotfModuleSpecifier *eotfModSpecifier,
        struct pqModuleSpecifier *pqModSpecifier);
    unsigned int getSourceLuminance(
        struct _HdrLayerInfo_ *layer);
public:
    hdr10Coef (void);
    void parse(hdrHwInfo *hwInfo,
                struct hdrContext *ctx);
    int coefBuildup(int layer_index,
                struct hdrContext *out);
    void init(struct hdrContext *ctx);
};

#endif
