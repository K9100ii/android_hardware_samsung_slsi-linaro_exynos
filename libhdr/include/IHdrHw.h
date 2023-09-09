#ifndef __I_HDR_HW_H__
#define __I_HDR_HW_H__

#include<string>
#include<vector>

class IHdrHw {
public:
    IHdrHw(void) { return; }
    virtual ~IHdrHw(void) { return; }
    virtual void parse(std::string __attribute__((unused)) &hdrInfoId, std::string __attribute__((unused)) &filename) { return; }
    virtual void dump(void) { return; }
    virtual int getLayerNum(void) { return 0; }
    virtual int getHdrCoefSize(void) { return 0; }
    virtual bool hasMod(int __attribute__((unused)) layer) { return false; }
    virtual bool hasSubMod(int __attribute__((unused)) layer, std::string __attribute__((unused)) &subModName) { return false; }
    virtual int getSubModNodeNum(std::string __attribute__((unused)) &subModName, int __attribute__((unused)) layer) { return 0; }
    virtual int pack(std::string __attribute__((unused)) &subModName, int __attribute__((unused)) layer,
            std::vector<int> __attribute__((unused)) &in, struct hdr_dat_node __attribute__((unused)) &out) { return 0; }
};

#endif
