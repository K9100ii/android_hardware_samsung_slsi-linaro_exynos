#ifndef __HDR_MODULE_CLASSIFIER_H__
#define __HDR_MODULE_CLASSIFIER_H__

#include <hardware/exynos/hdrInterface.h>
#include "hdrHwInfo.h"

struct tonemapModuleSpecifier {
    bool has;
    std::vector<int> mod_en;
    int size_x;
    std::vector<int> mod_x;
    int size_y;
    std::vector<int> mod_y;
    std::string mod_en_id;
    std::string mod_x_id;
    std::string mod_y_id;
    int mod_x_bit;
    int mod_y_bit;
    int mod_minx_bit;
    struct hdr_dat_node mod_en_packed;
    struct hdr_dat_node mod_x_packed;
    struct hdr_dat_node mod_y_packed;
    void init(void) {
        mod_en.clear();
        mod_x.clear();
        mod_y.clear();
        mod_en_id.clear();
        mod_x_id.clear();
        mod_y_id.clear();
        size_x = 0;
        size_y = 0;
    }
    bool isValidNit(int max_nit)
    {
        if (max_nit <= 0)
            return false;
        return true;
    }
};

struct pqModuleSpecifier {
    bool has;
    std::vector<int> mod_en;
    std::vector<int> pq_en;
    std::vector<int> coef;
    std::vector<int> shift;
    std::string mod_en_id;
    std::string pq_en_id;
    std::string coef_id;
    std::string shift_id;
    struct hdr_dat_node mod_en_packed;
    struct hdr_dat_node pq_en_packed;
    struct hdr_dat_node coef_packed;
    struct hdr_dat_node shift_packed;
    void init(void) {
        has = false;
        mod_en.clear();
        pq_en.clear();
        coef.clear();
        shift.clear();
    }
    double get_SMPTE_PQ (double N)
    {
        double m1 = 0.1593017578125;
        double m2 = 78.84375;
        double c2 = 18.8515625;
        double c3 = 18.6875;
        double c1 = c3 - c2 + 1.0;

        return std::pow((fmax((std::pow(N, (1/m2)))-c1, 0)
                    / (c2 - c3 * (std::pow(N, (1/m2)))))
                ,(1/m1));
    }
    bool isValidNit(int max_nit)
    {
        if (max_nit <= 0 || max_nit > 4000)
            return false;
        return true;
    }
    void genEotfPQ(int max_nit)
    {
        if (!isValidNit(max_nit))
            return;

        int normalize = (int)((get_SMPTE_PQ(924.0/1023.0)
                    / (max_nit/10000.0))
                * 65536);
        mod_en[0] = 1;
        pq_en[0] = 1;
        shift[0] = 0;
        coef[0] = 0;

        for (int exp = 21; exp >= 0; exp--) {
            if (normalize >= std::pow(2, exp)) {
                shift[0] = 16 - exp;
                break;
            }
        }

        if (shift[0] < 0)
            coef[0] = (normalize >> (0 - shift[0])) - 65536;
        else
            coef[0] = (normalize << shift[0]) - 65536;
        shift[0] += 16;
    }
};

struct eotfModuleSpecifier {
    bool has;
    std::vector<int> mod_en;
    int size_x;
    std::vector<int> mod_x;
    int size_y;
    std::vector<int> mod_y;
    std::string mod_en_id;
    std::string mod_x_id;
    std::string mod_y_id;
    int mod_x_bit;
    int mod_y_bit;
    int mod_minx_bit;
    struct hdr_dat_node mod_en_packed;
    struct hdr_dat_node mod_x_packed;
    struct hdr_dat_node mod_y_packed;
    void init(void) {
        mod_en.clear();
        mod_x.clear();
        mod_y.clear();
        mod_en_id.clear();
        mod_x_id.clear();
        mod_y_id.clear();
        size_x = 0;
        size_y = 0;
    }
    bool isValidNit(int max_nit)
    {
        if (max_nit <= 0)
            return false;
        return true;
    }
};

struct hdrModuleSpecifier {
    struct tonemapModuleSpecifier tmModSpecifier;
    struct pqModuleSpecifier pqModSpecifier;
    struct eotfModuleSpecifier eotfModSpecifier;
};

class hdrModuleSpecifiers {
private:
    std::string filename = "/vendor/etc/dqe/hdrModuleSpecifiers.xml";
    std::unordered_map<int, struct hdrModuleSpecifier> layerToHdrModSpecifier;
    hdrHwInfo *hwInfo = NULL;

    void parse_pqModSpecifier(
            int module_id,
            xmlDocPtr xml_doc,
            xmlNodePtr xml_pq,
            struct pqModuleSpecifier &pqMod);
    void parse_tonemapModSpecifier(
            int module_id,
            xmlDocPtr xml_doc,
            xmlNodePtr xml_tm,
            struct tonemapModuleSpecifier &tmMod);
    void parse_eotfModSpecifier(
            int module_id,
            xmlDocPtr xml_doc,
            xmlNodePtr xml_eotf,
            struct eotfModuleSpecifier &eotfMod);
    void parse_hdrModuleSpecifier(
            int module_id,
            xmlDocPtr xml_doc,
            xmlNodePtr xml_mod,
            struct hdrModuleSpecifier &hdrModSpecifier);
    void parse_hdrModuleSpecifiers(
            xmlDocPtr xml_doc,
            xmlNodePtr xml_modInfo);
public:
    void init(hdrHwInfo *hwInfo);
    struct tonemapModuleSpecifier *getTonemapModuleSpecifier(int layer_index);
    struct pqModuleSpecifier *getPqModuleSpecifier(int layer_index);
    struct eotfModuleSpecifier *getEotfModuleSpecifier(int layer_index);
};

#endif
