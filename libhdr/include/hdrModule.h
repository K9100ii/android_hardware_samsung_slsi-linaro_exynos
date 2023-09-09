#ifndef __HDR_MODULE_H__
#define __HDR_MODULE_H__
#include <deque>
#include <string>
#include <unordered_map>
#include "hdrUtil.h"
#include "libhdr_parcel_header.h"

struct hdrFunction {
    int size = 5;
    std::string hdrFuncsId[5] = { "wcg", "hdr10", "hdr10p", "hlg", "dolby" };
    bool hdrFuncs[5];
};

struct hdrBpc {
    int size = 3;
    std::string hdrBpcsId[3] = { "b8", "b10", "fp16" };
    bool hdrBpcs[3];
};

struct hdrExFunc {
    std::string modEn;
    std::vector<std::string> subModEn;
    std::vector<std::string> subMod;
    ~hdrExFunc(void) {
        modEn.clear();
        subModEn.clear();
        subMod.clear();
    }
    void init(void) {
        modEn.clear();
        subModEn.clear();
        subMod.clear();
    }
};

struct hdrExFunction {
    int size = 2;
    std::string hdrExFuncsId[2] = { "tf_matrix", "sdr_dimming" };
    struct hdrExFunc hdrExFuncs[2];
    void init(void) {
        for (int i = 0; i < 2; i++)
            hdrExFuncs[i].init();
    }
};

struct hdrModuleCapabilities {
    struct hdrFunction hdrFuncs;
    std::unordered_map<std::string, bool*> hdrFuncsMap;

    struct hdrBpc hdrBpcs;
    std::unordered_map<std::string, bool*> hdrBpcsMap;

    struct hdrExFunction hdrExFuncs;
    std::unordered_map<std::string, struct hdrExFunc*> hdrExFuncsMap;

    ~hdrModuleCapabilities(void) {
        hdrFuncsMap.clear();
        hdrBpcsMap.clear();
        hdrExFuncsMap.clear();
    }
    void init (void) {
        hdrFuncsMap.clear();
        hdrBpcsMap.clear();
        hdrExFuncsMap.clear();
        hdrExFuncs.init();
        for (int i = 0; i < hdrFuncs.size; i++)
            hdrFuncsMap[hdrFuncs.hdrFuncsId[i]] = &hdrFuncs.hdrFuncs[i];
        for (int i = 0; i < hdrBpcs.size; i++)
            hdrBpcsMap[hdrBpcs.hdrBpcsId[i]] = &hdrBpcs.hdrBpcs[i];
        for (int i = 0; i < hdrExFuncs.size; i++)
            hdrExFuncsMap[hdrExFuncs.hdrExFuncsId[i]] = &hdrExFuncs.hdrExFuncs[i];
    }
    void dump(int level) {
        level++;
    }
};

struct hdrSubModule {
    int numNodes = 0;
    int numNodesPerReg = 0;
    bool lastNodeAlign = false;
    std::vector<unsigned int> bitOffsets;
    std::vector<unsigned int> masks;
    int regOffset = 0;
    int regNum = 0;
    /* -1 when no group */
    int groupId = -1;
    ~hdrSubModule(void) {
        bitOffsets.clear();
        masks.clear();
    }
    void init (void) {
        numNodes = 0;
        numNodesPerReg = 0;
        lastNodeAlign = false;
        bitOffsets.clear();
        masks.clear();
        regOffset = 0;
        regNum = 0;
        groupId = -1;
    }
    void dump(int level) {
        TAB(level); ALOGD("numNodes(%d), numNodesPerReg(%d), lastNodeAlign(%d)",
                                numNodes, numNodesPerReg, (int)lastNodeAlign);
        TAB(level);
        ALOGD("bitOffsets( ");
        for (auto &v: bitOffsets)
            ALOGD("%6d ", v);
        ALOGD(")");
        TAB(level);
        ALOGD("masks     ( ");
        for (auto &v: masks)
            ALOGD("0x%04x ", v);
        ALOGD(")");
        TAB(level); ALOGD("regNum(%d), regOffset(%d), groupId(%d)",
                                regNum, regOffset, groupId);
    }
    int pack(std::vector<int> &in, struct hdr_dat_node &out)
    {
        if (in.size() != numNodes) {
            ALOGE("pack()::wrong number of nodes (num[wcg.xml]=%lu, num[hw.xml]=%d)",
                    (unsigned long)in.size(), numNodes);
            return -1;
        }

        out.set_header(regOffset, regNum);
        out.set_data(in, numNodes, numNodesPerReg, lastNodeAlign, bitOffsets, masks);
        out.set_group_id(groupId);
        return 0;
    }
};

struct hdrSubModules {
    std::unordered_map<std::string, struct hdrSubModule> nameToHdrSubMod;
    ~hdrSubModules(void) {
        nameToHdrSubMod.clear();
    }
    void init(void) {
        nameToHdrSubMod.clear();
    }
    void dump(int level) {
        TAB(level); ALOGD("hdrSubModules >");
        level++;
        TAB(level); ALOGD("[hdrSubMod]");
        TAB(level); ALOGD("[nameToHdrSubMod]");
        level++;
        for (auto &m: nameToHdrSubMod) {
            TAB(level);
            ALOGD("submod_name(%s)", m.first.c_str());
            m.second.dump(level);
        }
    }
};

struct hdrModule {
    int id;
    hdrModuleCapabilities       hdrModCapa;
    hdrSubModules               hdrSubMods;
    hdrModule(void) {
        id = -1;
        hdrModCapa.init();
        hdrSubMods.init();
    }
    void init(void) {
        hdrModCapa.init();
        hdrSubMods.init();
    }
    void dump(void) {
        ALOGD("hdrModule (id = %d) >", id);
        int level = 1;
        hdrModCapa.dump(level);
        hdrSubMods.dump(level);
    }
};

#endif
