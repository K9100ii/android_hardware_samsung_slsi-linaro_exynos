#ifndef __WCG_COEF_H__
#define __WCG_COEF_H__
#include <system/graphics.h>
#include <unordered_map>
#include <vector>
#include "libhdr_parcel_header.h"
#include "hdrUtil.h"
#include "hdrHwInfo.h"

class wcgCoef {
private:
    struct strToIdNode {
        std::string str;
        int id;
    };
    struct strToIdNode transferTable[9] = {
        {"UNSPECIFIED",     HAL_DATASPACE_TRANSFER_UNSPECIFIED >> HAL_DATASPACE_TRANSFER_SHIFT},
        {"LINEAR",          HAL_DATASPACE_TRANSFER_LINEAR      >> HAL_DATASPACE_TRANSFER_SHIFT},
        {"SRGB",            HAL_DATASPACE_TRANSFER_SRGB        >> HAL_DATASPACE_TRANSFER_SHIFT},
        {"SMPTE_170M",      HAL_DATASPACE_TRANSFER_SMPTE_170M  >> HAL_DATASPACE_TRANSFER_SHIFT},
        {"GAMMA2_2",        HAL_DATASPACE_TRANSFER_GAMMA2_2    >> HAL_DATASPACE_TRANSFER_SHIFT},
        {"GAMMA2_6",        HAL_DATASPACE_TRANSFER_GAMMA2_6    >> HAL_DATASPACE_TRANSFER_SHIFT},
        {"GAMMA2_8",        HAL_DATASPACE_TRANSFER_GAMMA2_8    >> HAL_DATASPACE_TRANSFER_SHIFT},
        {"ST2084",          HAL_DATASPACE_TRANSFER_ST2084      >> HAL_DATASPACE_TRANSFER_SHIFT},
        {"HLG",             HAL_DATASPACE_TRANSFER_HLG         >> HAL_DATASPACE_TRANSFER_SHIFT},
    };
    struct strToIdNode standardTable[12] = {
        {"UNSPECIFIED",             HAL_DATASPACE_STANDARD_UNSPECIFIED                  >> HAL_DATASPACE_STANDARD_SHIFT},
        {"BT709",                   HAL_DATASPACE_STANDARD_BT709                        >> HAL_DATASPACE_STANDARD_SHIFT},
        {"BT601_625",               HAL_DATASPACE_STANDARD_BT601_625                    >> HAL_DATASPACE_STANDARD_SHIFT},
        {"BT601_625_UNADJUSTED",    HAL_DATASPACE_STANDARD_BT601_625_UNADJUSTED         >> HAL_DATASPACE_STANDARD_SHIFT},
        {"BT601_525",               HAL_DATASPACE_STANDARD_BT601_525                    >> HAL_DATASPACE_STANDARD_SHIFT},
        {"BT601_525_UNADJUSTED",    HAL_DATASPACE_STANDARD_BT601_525_UNADJUSTED         >> HAL_DATASPACE_STANDARD_SHIFT},
        {"BT2020",                  HAL_DATASPACE_STANDARD_BT2020                       >> HAL_DATASPACE_STANDARD_SHIFT},
        {"BT2020_CONSTANT_LUMINANCE",HAL_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE   >> HAL_DATASPACE_STANDARD_SHIFT},
        {"BT470M",                  HAL_DATASPACE_STANDARD_BT470M                       >> HAL_DATASPACE_STANDARD_SHIFT},
        {"FILM",                    HAL_DATASPACE_STANDARD_FILM                         >> HAL_DATASPACE_STANDARD_SHIFT},
        {"DCI_P3",                  HAL_DATASPACE_STANDARD_DCI_P3                       >> HAL_DATASPACE_STANDARD_SHIFT},
        {"ADOBE_RGB",               HAL_DATASPACE_STANDARD_ADOBE_RGB                    >> HAL_DATASPACE_STANDARD_SHIFT},
    };

    struct eotf {
        std::vector<int> eotfCoef;
        struct hdr_dat_node eotfCoef_packed;
        eotf() {
            eotfCoef.clear();
        }
        eotf(const struct eotf &op) {
            eotfCoef = op.eotfCoef;
            eotfCoef_packed = op.eotfCoef_packed;
        }
        struct eotf &operator=(const struct eotf &op) {
            eotfCoef = op.eotfCoef;
            eotfCoef_packed = op.eotfCoef_packed;
            return *this;
        }
        void dump(int level) {
            TAB(level); ALOGD("eotfCoef[]"); EOL(level);
            int i = 0;
            TAB(level);
            for (auto n : eotfCoef) {
                ALOGD("0x%08x,", n);
                i++;
                if (i % 10 == 0) {
                    ALOGD("|"); EOL(level); TAB(level);
                }
            }
            ALOGD("|"); EOL(level);
            eotfCoef_packed.dump(level);
        }
    };
    struct eotfNode {
        /* ex) "eotf-y" -> struct eotf */
        std::unordered_map<std::string, struct eotf> data;
        eotfNode() {
            data.clear();
        }
        ~eotfNode() {
            data.clear();
        }
        eotfNode(const struct eotfNode &op) {
            data = op.data;
        }
        void dump(int level) {
            for (auto iter = data.begin(); iter != data.end(); iter++) {
                TAB(level); ALOGD("module(%s)", iter->first.c_str()); EOL(level);
                iter->second.dump(level+1);
            }
        }
    };
    struct oetf {
        std::vector<int> oetfCoef;
        struct hdr_dat_node oetfCoef_packed;
        oetf() {
            oetfCoef.clear();
        }
        ~oetf() {
            oetfCoef.clear();
        }
        oetf(const struct oetf &op) {
            oetfCoef = op.oetfCoef;
            oetfCoef_packed = op.oetfCoef_packed;
        }
        struct oetf &operator=(const struct oetf &op) {
            oetfCoef = op.oetfCoef;
            oetfCoef_packed = op.oetfCoef_packed;
            return *this;
        }
        void dump(int level) {
            TAB(level); ALOGD("oetfCoef[]"); EOL(level);
            int i = 0;
            TAB(level);
            for (auto n : oetfCoef) {
                ALOGD("0x%08x,", n);
                i++;
                if (i % 10 == 0) {
                    ALOGD("|"); EOL(level); TAB(level);
                }
            }
            ALOGD("|"); EOL(level);
            oetfCoef_packed.dump(level);
        }
    };
    struct oetfNode {
        /* ex) "oetf-x" -> struct oetf */
        std::unordered_map<std::string, struct oetf> data;
        oetfNode() {
            data.clear();
        }
        ~oetfNode() {
            data.clear();
        }
        oetfNode(const struct oetfNode &op) {
            data = op.data;
        }
        void dump(int level) {
            for (auto iter = data.begin(); iter != data.end(); iter++) {
                TAB(level); ALOGD("module(%s)", iter->first.c_str()); EOL(level);
                iter->second.dump(level+1);
            }
        }
    };
    struct gm {
        std::vector<int> gmCoef;
        struct hdr_dat_node gmCoef_packed;
        gm() {
            gmCoef.clear();
        }
        ~gm() {
            gmCoef.clear();
        }
        gm(const struct gm &op) {
            gmCoef = op.gmCoef;
            gmCoef_packed = op.gmCoef_packed;
        }
        struct gm &operator=(const struct gm &op) {
            gmCoef = op.gmCoef;
            gmCoef_packed = op.gmCoef_packed;
            return *this;
        }
        void dump(int level) {
            TAB(level);
            ALOGD("gmCoef->"); EOL(level);
            int i = 0;
            TAB(level);
            for (auto n : gmCoef) {
                ALOGD("0x%08x,", n);
                i++;
                if (i % 10 == 0) {
                    ALOGD("\n"); TAB(level);
                }
            }
            ALOGD("\n");
            TAB(level); ALOGD("gmCoef_packed->"); EOL(level);
            gmCoef_packed.dump(level);
        }
    };
    struct gmOutNode {
        /* ex) "gm-en" -> struct gm */
        std::unordered_map<std::string, struct gm> data;
        gmOutNode() {
            data.clear();
        }
        ~gmOutNode() {
            data.clear();
        }
        gmOutNode(const struct gmOutNode &op) {
            data = op.data;
        }
        void dump(int level) {
            for (auto iter = data.begin(); iter != data.end(); iter++) {
                TAB(level); ALOGD("module(%s)", iter->first.c_str()); EOL(level);
                iter->second.dump(level+1);
            }
        }
    };
    struct gmInOutNode {
        std::vector<struct gmOutNode> out;
        gmInOutNode() {
            out.resize(ARRSIZE(standardTable));
            //out.clear();
        }
        ~gmInOutNode() {
            out.clear();
        }
        gmInOutNode(const struct gmInOutNode &op) {
            out = op.out;
        }
        void dump(int level) {
            for (int i = 0; i < out.size(); i++) {
                TAB(level); ALOGD("<%d>", i); EOL(level);
                out[i].dump(level);
            }
        }
    };
    struct modEn {
        bool modEnCoef = false;
        struct hdr_dat_node modEnCoef_packed;
        modEn() {
        }
        ~modEn() {
        }
        modEn(const struct modEn &op) {
            modEnCoef = op.modEnCoef;
            modEnCoef_packed = op.modEnCoef_packed;
        }
        struct modEn &operator=(const struct modEn &op) {
            modEnCoef = op.modEnCoef;
            modEnCoef_packed = op.modEnCoef_packed;
            return *this;
        }
        void dump(int level) {
            TAB(level); ALOGD("modEn(%d)", modEnCoef); EOL(level);
            modEnCoef_packed.dump(level);
        }
    };
    struct modEnNode {
        std::unordered_map<std::string, struct modEn> data;
        modEnNode() {
            data.clear();
        }
        ~modEnNode() {
            data.clear();
        }
        modEnNode(const struct modEnNode &op) {
            data = op.data;
        }
        void dump(int level) {
            for (auto iter = data.begin(); iter != data.end(); iter++) {
                TAB(level); ALOGD("module(%s)", iter->first.c_str()); EOL(level);
                iter->second.dump(level+1);
            }
        }
    };
    struct customOutNode {
        int dataspace;
        int capa;
    };
    struct wcgModule {
        struct strToIdNode __transferTable[9] = {
            {"UNSPECIFIED",     HAL_DATASPACE_TRANSFER_UNSPECIFIED >> HAL_DATASPACE_TRANSFER_SHIFT},
            {"LINEAR",          HAL_DATASPACE_TRANSFER_LINEAR      >> HAL_DATASPACE_TRANSFER_SHIFT},
            {"SRGB",            HAL_DATASPACE_TRANSFER_SRGB        >> HAL_DATASPACE_TRANSFER_SHIFT},
            {"SMPTE_170M",      HAL_DATASPACE_TRANSFER_SMPTE_170M  >> HAL_DATASPACE_TRANSFER_SHIFT},
            {"GAMMA2_2",        HAL_DATASPACE_TRANSFER_GAMMA2_2    >> HAL_DATASPACE_TRANSFER_SHIFT},
            {"GAMMA2_6",        HAL_DATASPACE_TRANSFER_GAMMA2_6    >> HAL_DATASPACE_TRANSFER_SHIFT},
            {"GAMMA2_8",        HAL_DATASPACE_TRANSFER_GAMMA2_8    >> HAL_DATASPACE_TRANSFER_SHIFT},
            {"ST2084",          HAL_DATASPACE_TRANSFER_ST2084      >> HAL_DATASPACE_TRANSFER_SHIFT},
            {"HLG",             HAL_DATASPACE_TRANSFER_HLG         >> HAL_DATASPACE_TRANSFER_SHIFT},
        };
        struct strToIdNode __standardTable[12] = {
            {"UNSPECIFIED",             HAL_DATASPACE_STANDARD_UNSPECIFIED                  >> HAL_DATASPACE_STANDARD_SHIFT},
            {"BT709",                   HAL_DATASPACE_STANDARD_BT709                        >> HAL_DATASPACE_STANDARD_SHIFT},
            {"BT601_625",               HAL_DATASPACE_STANDARD_BT601_625                    >> HAL_DATASPACE_STANDARD_SHIFT},
            {"BT601_625_UNADJUSTED",    HAL_DATASPACE_STANDARD_BT601_625_UNADJUSTED         >> HAL_DATASPACE_STANDARD_SHIFT},
            {"BT601_525",               HAL_DATASPACE_STANDARD_BT601_525                    >> HAL_DATASPACE_STANDARD_SHIFT},
            {"BT601_525_UNADJUSTED",    HAL_DATASPACE_STANDARD_BT601_525_UNADJUSTED         >> HAL_DATASPACE_STANDARD_SHIFT},
            {"BT2020",                  HAL_DATASPACE_STANDARD_BT2020                       >> HAL_DATASPACE_STANDARD_SHIFT},
            {"BT2020_CONSTANT_LUMINANCE",HAL_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE   >> HAL_DATASPACE_STANDARD_SHIFT},
            {"BT470M",                  HAL_DATASPACE_STANDARD_BT470M                       >> HAL_DATASPACE_STANDARD_SHIFT},
            {"FILM",                    HAL_DATASPACE_STANDARD_FILM                         >> HAL_DATASPACE_STANDARD_SHIFT},
            {"DCI_P3",                  HAL_DATASPACE_STANDARD_DCI_P3                       >> HAL_DATASPACE_STANDARD_SHIFT},
            {"ADOBE_RGB",               HAL_DATASPACE_STANDARD_ADOBE_RGB                    >> HAL_DATASPACE_STANDARD_SHIFT},
        };

        std::vector<struct modEnNode>       modEnTable;     // 0/1 -> on/off -> true/false order
        std::vector<struct eotfNode>        eotfTable;      // number of eotf Nodes = tfStrMap.size();
        std::vector<struct oetfNode>        oetfTable;      // number of oetf Nodes = tfStrMap.size();
        std::vector<struct gmInOutNode>     gmTable;        // number of gmNode = stStrMap.size();
        std::unordered_map<int, struct customOutNode> customTable; // in ds / out custom

        wcgModule() {
            modEnTable.resize(2);
            eotfTable.resize(ARRSIZE(transferTable));
            oetfTable.resize(ARRSIZE(transferTable));
            gmTable.resize(ARRSIZE(standardTable));
            //modEnTable.clear();
            //eotfTable.clear();
            //oetfTable.clear();
            //gmTable.clear();
            customTable.clear();
        }
        wcgModule(const struct wcgModule &op) {
            this->modEnTable            = op.modEnTable;
            this->eotfTable             = op.eotfTable;
            this->oetfTable             = op.oetfTable;
            this->gmTable               = op.gmTable;
            this->customTable           = op.customTable;
        }
        struct wcgModule &operator=(const struct wcgModule &op) {
            this->modEnTable            = op.modEnTable;
            this->eotfTable             = op.eotfTable;
            this->oetfTable             = op.oetfTable;
            this->gmTable               = op.gmTable;
            this->customTable           = op.customTable;
            return *this;
        }
        void dump(int level) {
            TAB(level); ALOGD("modEnTable[]"); EOL(level);
            for (int i = 0; i < modEnTable.size(); i++) {
                TAB(level); ALOGD("%d >", i);
                modEnTable[i].dump(level+1);
            }
            TAB(level); ALOGD("eotfTable[]"); EOL(level);
            for (int i = 0; i < eotfTable.size(); i++) {
                TAB(level); ALOGD("> transfer function(%s)", __transferTable[i].str.c_str());
                eotfTable[i].dump(level+1);
            }
            TAB(level); ALOGD("oetfTable[]"); EOL(level);
            for (int i = 0; i < oetfTable.size(); i++) {
                TAB(level); ALOGD("> transfer function(%s)", __transferTable[i].str.c_str());
                oetfTable[i].dump(level+1);
            }
            TAB(level); ALOGD("gmTable[]"); EOL(level);
            for (int i = 0; i < gmTable.size(); i++) {
                TAB(level); ALOGD("> in standard(%s)", __standardTable[i].str.c_str());
                gmTable[i].dump(level+1);
            }
        }
    };
    std::unordered_map<int, struct wcgModule> layerToWcgMod;
    std::unordered_map<std::string, int> tfStrMap;
    std::unordered_map<std::string, int> stStrMap;
    std::unordered_map<std::string, int> capStrMap;
    std::string filename = "/vendor/etc/dqe/wcgLut";

    hdrHwInfo *hwInfo = NULL;

    void parse(std::vector<struct supportedHdrHw> *list, struct hdrContext *ctx);
    void __parse__(int hw_id, struct hdrContext *ctx);
    void parse_wcgMods(
        int hw_id,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module);
    void parse_wcgMod(
        int hw_id,
        int module_id,
        struct wcgModule &wcg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module);
    void parse_inoutDataspace(
        int hw_id,
        int module_id,
        struct wcgModule &wcg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_option);
    void parse_inoutTransfer(
        int hw_id,
        int module_id,
        struct wcgModule &wcg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module);
    void parse_inoutGamut(
        int hw_id,
        int module_id,
        struct wcgModule &wcg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_in);
    void parse_inTransfer(
        int hw_id,
        int module_id,
        struct wcgModule &wcg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_in);
    void parse_outTransfer(
        int hw_id,
        int module_id,
        struct wcgModule &wcg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_out);
    void parse_inoutCustom(
        struct wcgModule &wcg_module,
        xmlNodePtr xml_in);
    void dump(void) {
        int i = 0;
        for (auto iter = layerToWcgMod.begin(); iter != layerToWcgMod.end(); iter++, i++) {
            ALOGD("layer[%d]", iter->first);
            iter->second.dump(1);
            if (i == 0)
                break;
        }
        return;
    }
    void dump(int layer) {
        ALOGD("layer[%d]", layer);
        layerToWcgMod[layer].dump(1);
        return;
    }
public:
    void init(hdrHwInfo *hwInfo,
                struct hdrContext *ctx);
    int coefBuildup(int layer_index,
                struct hdrContext *out);
};

#endif
