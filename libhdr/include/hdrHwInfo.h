#ifndef __HDR_HW_INFO_H__
#define __HDR_HW_INFO_H__
#include <hardware/exynos/hdrInterface.h>
#include <unordered_map>
#include <string>
#include "hdrHwDPU.h"

struct supportedHdrHw {
    enum HdrHwId id;
    std::string strId;
    std::string hdrHwInfoFile;
    IHdrHw *IHw;
};

class hdrHwInfo {
private:
    /* supported hdr hardware list */
    std::string hdrHwInfoid = "hdr_hw";
    class hdrHwDPU hwDPU;
    std::vector<struct supportedHdrHw> listHdrHw = {
        /* HDR_HW_DPU == 0 */
        {HDR_HW_DPU, "DPU", "/vendor/etc/dqe/hdrHwDPU.xml", &hwDPU},
    };

    std::unordered_map<std::string, int> strIdToId;
    std::unordered_map<int, std::string> idToStrId;
    std::unordered_map<int, std::string> idToFileName;
    std::unordered_map<int ,IHdrHw*> idToIHdr;
public:
    void init(void);
    IHdrHw *getIf (int hw_id);
    std::vector<struct supportedHdrHw> *getListHdrHw(void);
};

#endif
