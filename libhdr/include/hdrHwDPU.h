#ifndef __HW_HDR_DPU_H__
#define __HW_HDR_DPU_H__
#include "IHdrHw.h"
#include "hdrModule.h"
#include "hdrUtil.h"
#include <string>
#include <deque>
#include <unordered_map>

class hdrHwDPU: public IHdrHw {
    std::string hdrHwId = "DPU";

    std::unordered_map<int, struct hdrModule> layerToHdrMod;
    int coefSize = -1;

    void parse_module(
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module);
public:
    hdrHwDPU(void) { return; }
    ~hdrHwDPU(void) { return; }
    void parse(std::string &hdrInfoId, std::string &filename);
    void dump(void);
    int getLayerNum(void);
    bool hasMod(int layer);
    bool hasSubMod(int layer, std::string &subModName);
    int getSubModNodeNum(std::string &subModName, int layer);
    int getHdrCoefSize(void);
    int pack(std::string &subModName, int layer,
            std::vector<int> &in, struct hdr_dat_node &out);
};

#endif
