#ifndef __HDR_UTIL_H__
#define __HDR_UTIL_H__

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <utils/Log.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <system/graphics.h>
#include <hardware/exynos/hdrInterface.h>
#include <unordered_map>

typedef unsigned int u32;
typedef int s32;

#define NO_HW           "NO_HW"
#define NO_FILE         "NO_FILE"
#define ARRSIZE(arr)	(sizeof(arr) / sizeof(arr[0]))

#ifndef PROTO_TYPE
#define TAB(L)                                  \
    do {                                        \
        L = 0;                                  \
    } while(0)                                  \
        //printf("tag level : %d\n", L);

#define EOL(L)                                  \
    do {                                        \
        L = 0;                                  \
    } while(0)                                  \
        //printf("EOL\n");

#else
#define ALOGD printf
#define ALOGE printf
#define TAB(L)                                      \
    do {                                            \
        for(int i = 0; i < L; i++) printf("\t");    \
    } while(0)
#define EOL(L)     printf("\n");
#endif

#define LIBHDR_LOGD(log_level, fmt, ...)               \
    do {                                                \
        if (log_level > 0)                              \
            ALOGD(fmt, ##__VA_ARGS__);                  \
    } while (0)                                         \

void split(const std::string &s, const char delim, std::vector<std::string> &result);
void split(const std::string &s, const char delim, std::vector<int> &result);
unsigned int strToHex(std::string &&s);
unsigned int strToHex(std::string &s);

struct hdr_node {
    // keys
    int capa;
    int gamut;
    int transfer_function;
    // data
    std::string filename;
};

struct hdrInfo {
    std::string infofile;
    std::string defaultfile;
    std::vector<struct hdr_node> hdr_nodes;
    std::string getFileName(struct HdrTargetInfo *tInfo) {
        int capa = tInfo->hdr_capa;
        int gamut = (tInfo->dataspace & HAL_DATASPACE_STANDARD_MASK) >> HAL_DATASPACE_STANDARD_SHIFT;
        int transfer = (tInfo->dataspace & HAL_DATASPACE_TRANSFER_MASK) >> HAL_DATASPACE_TRANSFER_SHIFT;
        ALOGD("getFileName() keys:target capa(%d), gamut(%d), transfer(%d)",
                capa, gamut, transfer);
        for (auto &v: hdr_nodes) {
            if ((v.capa == HDR_CAPA_MAX || v.capa == capa) &&
                (v.gamut == 0 || v.gamut == gamut) &&
                (v.transfer_function == 0 || v.transfer_function == transfer)) {
                ALOGD("hdr nodes capa(%d), gamut(%d), transfer(%d) found -> filename(%s)",
                        v.capa, v.gamut, v.transfer_function, v.filename.c_str());
                return v.filename;
            }
        }
        ALOGE("hdr nodes not found!!!!");
        return "";
    }
};

struct strToIdNode {
    std::string str;
    int id;
};

std::unordered_map<std::string, int>& mapStringToStandard(void);
std::unordered_map<std::string, int>& mapStringToTransfer(void);
std::unordered_map<std::string, int>& mapStringToCapa(void);

#endif

