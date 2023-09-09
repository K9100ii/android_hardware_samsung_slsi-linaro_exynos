#include "hdrUtil.h"

template<typename Out>
static void split(const std::string &s, char delim, Out result)
{
    std::stringstream ss(s);
    std::string item;

    while (getline(ss, item, delim))
        *(result++) = item;
}

void split(const std::string &s, const char delim, std::vector<std::string> &result)
{
    result.clear();

    split(s, delim, back_inserter(result));
}

void split(const std::string &s, const char delim, std::vector<int> &result)
{
    result.clear();

    std::vector<std::string> temp;
    split (s, delim, temp);

    for (auto &t : temp)
        result.push_back(stoi(t));
}

unsigned int strToHex(std::string &s)
{
    unsigned int hexVal;
    std::stringstream convert(s);
    convert >> std::hex >> hexVal;
    return hexVal;
}

unsigned int strToHex(std::string &&s)
{
    unsigned int hexVal;
    std::stringstream convert(s);
    convert >> std::hex >> hexVal;
    return hexVal;
}

static struct strToIdNode transferTable[9] = {
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

static struct strToIdNode standardTable[12] = {
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

static struct strToIdNode capaTable[12] = {
    {"UNSPECIFIED",             HDR_CAPA_MAX},
    {"INNER",                   HDR_CAPA_INNER},
    {"OUTER",                   HDR_CAPA_OUTER},
};

static std::unordered_map<std::string, int> tfStrMap;
std::unordered_map<std::string, int>& mapStringToTransfer(void) {
    if (tfStrMap.size() == 0) {
        tfStrMap.clear();
        for (auto &tf : transferTable)
            tfStrMap[tf.str] = tf.id;
    }
    return tfStrMap;
}

static std::unordered_map<std::string, int> stStrMap;
std::unordered_map<std::string, int>& mapStringToStandard(void) {
    if (stStrMap.size() == 0) {
        stStrMap.clear();
        for (auto &st : standardTable)
            stStrMap[st.str] = st.id;
    }
    return stStrMap;
}

static std::unordered_map<std::string, int> capaStrMap;
std::unordered_map<std::string, int>& mapStringToCapa(void) {
    if (capaStrMap.size() == 0) {
        capaStrMap.clear();
        for (auto &cap : capaTable)
            capaStrMap[cap.str] = cap.id;
    }
    return capaStrMap;
}

