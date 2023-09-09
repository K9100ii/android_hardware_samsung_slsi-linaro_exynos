#ifndef __HDR_TEST_VECTOR_H__
#define __HDR_TEST_VECTOR_H__
#include <system/graphics.h>
#include <libhdr_parcel_header.h>
#include <hdrUtil.h>
#include <hdrHwInfo.h>
#include <algorithm>
#include <unistd.h>

#include "VendorVideoAPI_hdrTest.h"
class hdrTestVector {
private:
    struct hdr10Node {
        int max_luminance;
        std::vector<struct hdr_dat_node> coef_packed;
    };
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
    std::string filename = "/vendor/etc/dqe/hdr10Lut.xml";
    std::unordered_map<int, struct hdr10Module> layerToHdr10Mod;
    std::vector<std::unordered_map<int, struct hdr_dat_node>> group_list[HDR_HW_MAX];
    void parse(std::vector<struct supportedHdrHw> *list);
    void __parse__(int hw_id);
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

    std::unordered_map<int,struct hdr_dat_node*> hdrTV_map;
public:
    std::string testimage = "/data/hdr_test_1.png";
    void init(hdrHwInfo *hwInfo);
    std::unordered_map<int,struct hdr_dat_node*>& getTestVector(
            int hw_id, int layer_index, ExynosHdrStaticInfo *s_meta);
};

#endif
