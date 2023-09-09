#ifndef __HDR10P_COEF_H__
#define __HDR10P_COEF_H__
#include <system/graphics.h>
#include "libhdr_parcel_header.h"
#include "hdrUtil.h"
#include "hdrHwInfo.h"
#include <algorithm>
#include "hdrModuleSpecifiers.h"

struct hdr10pNode {
    int max_luminance;
    std::vector<struct hdr_dat_node> coef_packed;
};

class hdr10pCoef {
private:
    const char* __attribute__((unused)) TAG = "hdr10pCoef";
    struct hdr10pModule {
        std::vector<struct hdr10pNode>        hdr10pNodeTable;
        void sort_ascending(void) {
            std::sort(hdr10pNodeTable.begin(), hdr10pNodeTable.end(),
                    [] (struct hdr10pNode &op1, struct hdr10pNode &op2) {
                        return op1.max_luminance < op2.max_luminance;
                    });
        }
    };
    hdrHwInfo *hwInfo = NULL;
    struct hdrInfo hdr10pinfo;
    std::unordered_map<std::string, int> capStrMap;
    std::unordered_map<std::string, int> stStrMap;
    std::unordered_map<std::string, int> tfStrMap;
    std::unordered_map<int, struct hdr10pModule> layerToHdr10pMod;
    void parse(std::vector<struct supportedHdrHw> *list, struct hdrContext *ctx);
    void __parse__(int hw_id, struct hdrContext *ctx);
    void parse_hdr10pMods(
        int hw_id,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module);
    void parse_hdr10pMod(
        int hw_id,
        int module_id,
        struct hdr10pModule &hdr10p_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module);
    void parse_hdr10pNode(
        int hw_id,
        int module_id,
        struct hdr10pNode __attribute__((unused)) &hdr10p_node,
        xmlDocPtr __attribute__((unused)) xml_doc,
        xmlNodePtr xml_hdr10pnode);
    void tm_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct tonemapModuleSpecifier *tmModSpecifier);
    void pq_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct pqModuleSpecifier *pqModSpecifier);
    void eotf_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct eotfModuleSpecifier *eotfModSpecifier,
        struct pqModuleSpecifier *pqModSpecifier);
    void parse_hdrNodes(
            xmlDocPtr xml_doc,
            xmlNodePtr xml_nodes);
    void parse_hdrSubNodes(
            xmlDocPtr xml_doc,
            xmlNodePtr xml_subnodes);
public:
    hdr10pCoef (void);
    void parse(hdrHwInfo *hwInfo,
                struct hdrContext *ctx);
    int coefBuildup(int layer_index,
                struct hdrContext *out);
    void init(struct hdrContext *ctx);
};

#endif
