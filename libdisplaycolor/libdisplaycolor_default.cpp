#include <hardware/exynos/libdisplaycolor.h>
#include <hardware/exynos/libdisplaycolor_drv.h>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <cmath>
#include <utils/Log.h>

#include <utils/Log.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <system/graphics.h>

#include <stdio.h>
#include <fcntl.h>

using std::vector;
using std::map;
using std::pair;
using std::make_pair;
using std::string;
using std::to_string;
using std::snprintf;

#define TRANSFORM_MATRIX_DATA_SIZE   (16 * 7)

const char *dqe_colormode_node[DQE_COLORMODE_ID_MAX] {
    NULL,
    "cgc17_enc",
    "cgc17_con",
    "cgc_dither",
    "degamma",
    "gamma",
    "gamma_matrix_test",
    "hsc48_lcg",
    "hsc",
    "scl",
    "de",
};

class DisplayColorImplementation : public IDisplayColor {
private:
    struct dqe_colormode_global_header gHeaderBase;
    //vector<vector<string>> DqeLutTable;
    //vector<vector<struct dqe_colormode_data_header> DqeLutHeaderTable;
    map< pair<uint32_t, uint32_t>, vector<string> > DqeLutMap;
    map< pair<uint32_t, uint32_t>, vector<struct dqe_colormode_data_header> > DqeLutHeaderMap;
    int DqeLutSize = TRANSFORM_MATRIX_DATA_SIZE + sizeof(dqe_colormode_data_header) + sizeof(dqe_colormode_global_header);
    vector<DisplayColorMode> CMList;
    map<uint32_t, vector<DisplayRenderIntent>> RIList;
    string CFMatrix;
    struct dqe_colormode_data_header CFMatrixHeader;
    bool init_completed = false;

    uint32_t mode = -1;
    uint32_t intent = -1;
    bool matrix_en = false;
    uint16_t crc_table[256];

    int log_level = 0;

    map<string, uint32_t> DqeNodeIdMap;

    void buildupDqeNodeNameToEnumMap(void) {
        for (int i = (int)DQE_COLORMODE_ID_CGC17_ENC;
                i < (int)DQE_COLORMODE_ID_MAX; i++) {
            DqeNodeIdMap.insert(make_pair(dqe_colormode_node[i],(uint32_t)i));
        }
    }
    int initUtilityVariables(void) {
        gHeaderBase.magic = DQE_COLORMODE_MAGIC;
        gHeaderBase.seq_num = 0;
        gHeaderBase.header_size = sizeof(dqe_colormode_global_header);
        gHeaderBase.total_size = 0;
        gHeaderBase.num_data = 0;
        gHeaderBase.crc = DQE_COLORMODE_MAGIC;
        gHeaderBase.reserved = 0;
        CFMatrixHeader.magic = (uint8_t)DQE_COLORMODE_MAGIC;
        CFMatrixHeader.id = (uint8_t)DQE_COLORMODE_ID_GAMMA_MATRIX;
        CFMatrixHeader.total_size = 0;
        CFMatrixHeader.header_size = (uint16_t)sizeof(struct dqe_colormode_data_header);
        CFMatrixHeader.crc = 0;
        buildupDqeNodeNameToEnumMap();
        genCrc16Table();
        return 0;
    }

    uint16_t genCrc16Table()
    {
        uint16_t poly = DQE_COLORMODE_MAGIC;
        uint16_t i, j, c;

        for (i = 0; i < 256; i++)
        {
            c = i;
            for (j = 0; j < 8; j++)
            {
                if (c & 1)
                    c = poly ^ (c >> 1);
                else
                    c >>= 1;
            }
            crc_table[i] = c;
        }

        return poly;
    }

    uint16_t getCrc16(const void* buf, int len)
    {
        uint16_t c = DQE_COLORMODE_MAGIC;
        const uint8_t* u = static_cast<const uint8_t*>(buf);

        for (int i = 0; i < len; ++i)
            c = crc_table[(c ^ u[i]) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFF;
    }

    xmlNodePtr parseSubXml(xmlDocPtr doc) {
        xmlNodePtr cur, node = NULL;

        cur = xmlDocGetRootElement(doc);
        if (cur == NULL) {
            ALOGE("empty document\n");
            return NULL;
        }

        if (xmlStrcmp(cur->name, (const xmlChar *)"Calib_Data")) {
            ALOGE("document of the wrong type, root node != Calib_Data\n");
            return NULL;
        }

        cur = cur->xmlChildrenNode;
        while (cur != NULL) {
            if ((!xmlStrcmp(cur->name, (const xmlChar *)"mode"))) {
                xmlChar *mode_name;

                mode_name = xmlGetProp(cur, (const xmlChar *)"name");
                if ((!xmlStrcmp(mode_name, (const xmlChar *)"tune")))
                    node = cur->xmlChildrenNode;
                if (mode_name != NULL)
                    xmlFree(mode_name);
            }
            cur = cur->next;
        }

        return node;
    }

    int buildupColorModeNodes(uint32_t display_id) {
        string COLOR_MODE_DOC_NAME = "/vendor/etc/dqe/calib_data_colormode";
        string DOC_EXT = ".xml";
        string DOC_POSTFIX = "";
#define TARGET_NAME_SYSFS_NODE	"/sys/class/dqe/dqe/xml"
#define DOC_LEN		(128)
        int target_name_fd;
        char target_name[DOC_LEN] = "\0";
        int rd_len;
        target_name_fd = open(TARGET_NAME_SYSFS_NODE, O_RDONLY);
        if (target_name_fd >= 0) {
            rd_len = read(target_name_fd, target_name, DOC_LEN);
            if (rd_len > 0 && rd_len < DOC_LEN) {
                target_name[rd_len] = '\0';
                string tName = target_name;
                tName.pop_back();
                ALOGD("target name(%s) confirmed", tName.c_str());
                DOC_POSTFIX = (string)"_";
                DOC_POSTFIX += tName;
            } else
                ALOGD("no target name is found(rd_len:%d)", rd_len);
            close(target_name_fd);
        } else
            ALOGD("no target name is found(fd(%d))", target_name_fd);

        string ID = to_string(display_id);
        string DOC_NAME = COLOR_MODE_DOC_NAME + ID + DOC_POSTFIX;
        string DOC_NAME_DEFAULT = COLOR_MODE_DOC_NAME + ID + DOC_EXT;
        auto GET_DOC_NAME_SUBXML = [=](string subxml) -> string {
            return (COLOR_MODE_DOC_NAME + ID + "_" + subxml + DOC_EXT);
        };
        xmlDocPtr doc;
        xmlNodePtr cur, node;
        xmlChar *key;

        doc = xmlParseFile(DOC_NAME.c_str());
        if (doc == NULL) {
            ALOGD("no document or can not parse the document(%s)",
                    DOC_NAME.c_str());
            doc = xmlParseFile(DOC_NAME_DEFAULT.c_str());
            if (doc == NULL) {
                ALOGD("no document or can not parse the default document(%s)",
                    DOC_NAME_DEFAULT.c_str());
                return -1;
            }
        }

        cur = xmlDocGetRootElement(doc);
        if (cur == NULL) {
            ALOGE("empty document\n");
            xmlFreeDoc(doc);
            return -1;
        }

        if (xmlStrcmp(cur->name, (const xmlChar *)"Calib_Data")) {
            ALOGE("document of the wrong type, root node != Calib_Data\n");
            xmlFreeDoc(doc);
            return -1;
        }

        cur = cur->xmlChildrenNode;

        while (cur != NULL) {
            xmlChar *mode_id = NULL, *mode_name = NULL;
            xmlChar *gamut_id = NULL, *gamut_name = NULL;
            xmlChar *intent_id = NULL, *intent_name = NULL;
            xmlChar *subXml = NULL;
            xmlDocPtr subdoc = NULL;

            if ((!xmlStrcmp(cur->name, (const xmlChar *)"mode"))) {
                uint32_t mId, gId, iId;
                string temp_id;

                mode_name = xmlGetProp(cur, (const xmlChar *)"name");
                mode_id = xmlGetProp(cur, (const xmlChar *)"mode_id");
                if (mode_name == NULL || mode_id == NULL)
                    goto next;
                temp_id = (const char*)mode_id;
                mId = stoi(temp_id);

                gamut_name = xmlGetProp(cur, (const xmlChar *)"gamut_name");
                gamut_id = xmlGetProp(cur, (const xmlChar *)"gamut_id");
                if (gamut_name == NULL || gamut_id == NULL)
                    goto next;
                temp_id = (const char*)gamut_id;
                gId = stoi(temp_id);

                intent_name = xmlGetProp(cur, (const xmlChar *)"intent_name");
                intent_id = xmlGetProp(cur, (const xmlChar *)"intent_id");
                if (intent_name == NULL || intent_id == NULL)
                    goto next;
                temp_id = (const char*)intent_id;
                iId = stoi(temp_id);

                DisplayColorMode tmpColorMode;
                DisplayRenderIntent tmpRenderIntent;

                tmpColorMode.modeId = (uint32_t)mId;
                tmpColorMode.gamutId = (uint32_t)gId;
                tmpColorMode.modeName = (const char*)mode_name;
                int i;
                for (i = 0; i < CMList.size(); i++)
                    if (tmpColorMode.modeId == CMList[i].modeId &&
                            tmpColorMode.gamutId == CMList[i].gamutId)
                        break;
                if (i == CMList.size())
                        CMList.push_back(tmpColorMode);

                tmpRenderIntent.intentId = (uint32_t)iId;
                tmpRenderIntent.intentName = (const char*)intent_name;
                if (RIList.find(tmpColorMode.modeId) == RIList.end()) {
                    vector<DisplayRenderIntent> newRIListNode;
                    newRIListNode.clear();
                    newRIListNode.push_back(tmpRenderIntent);
                    RIList.insert(make_pair(tmpColorMode.modeId, newRIListNode));
                } else {
                    for (i = 0; i < RIList[tmpColorMode.modeId].size(); i++)
                        if (tmpRenderIntent.intentId == RIList[tmpColorMode.modeId][i].intentId)
                            break;
                    if (i == RIList[tmpColorMode.modeId].size())
                        RIList[tmpColorMode.modeId].push_back(tmpRenderIntent);
                }

                node = cur->xmlChildrenNode;

                subXml = xmlGetProp(cur, (const xmlChar *)"subxml");
                if (subXml != NULL) {
                    string DOC_NAME_SUBXML = GET_DOC_NAME_SUBXML((const char*)subXml);

                    subdoc = xmlParseFile(DOC_NAME_SUBXML.c_str());
                    if (subdoc == NULL) {
                        ALOGD("no document or can not parse the document(%s)",
                                DOC_NAME_SUBXML.c_str());
                    } else {
                        node = parseSubXml(subdoc);
                        if (node == NULL)
                            node = cur->xmlChildrenNode;
                    }
                }

                vector<string> DqeLutTable_entry;
                vector<dqe_colormode_data_header> DqeLutHeaderTable_entry;

                int tf_matrix_size = sizeof(dqe_colormode_data_header) + TRANSFORM_MATRIX_DATA_SIZE;
                int tmp_size = sizeof(dqe_colormode_global_header) + tf_matrix_size;
                while (node != NULL) {
                    string nodeName = (const char*)node->name;
                    if (DqeNodeIdMap.count(nodeName) != 0) {
                        dqe_colormode_data_header tmp_data_header;
                        xmlChar *att0, *att1, *att2, *att3;
                        uint32_t att0_i = -1, att1_i = -1, att2_i = -1, att3_i = -1;
                        string tmp_att;
                        string tmp_data;

                        xmlChar *data;
                        data = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
                        tmp_data = (const char*)data;
                        xmlFree(data);

                        tmp_data_header.magic = (uint8_t)DQE_COLORMODE_MAGIC;
                        tmp_data_header.id = (uint8_t)DqeNodeIdMap[nodeName];
                        tmp_data_header.total_size = (uint16_t)(sizeof(struct dqe_colormode_data_header) + (tmp_data.size() + 1));
                        tmp_data_header.header_size = (uint16_t)sizeof(struct dqe_colormode_data_header);
                        tmp_data_header.crc = getCrc16((char*)tmp_data.c_str(), (tmp_data.size() + 1));

                        tmp_size += tmp_data_header.total_size;

                        att0 = xmlGetProp(node, (const xmlChar *)"att0");
                        if (att0 != NULL) {
                            tmp_att = (const char*)att0;
                            att0_i = (uint32_t)stoi(tmp_att);
                            xmlFree(att0);
                        }
                        att1 = xmlGetProp(node, (const xmlChar *)"att1");
                        if (att1 != NULL) {
                            tmp_att = (const char*)att1;
                            att1_i = (uint32_t)stoi(tmp_att);
                            xmlFree(att1);
                        }
                        att2 = xmlGetProp(node, (const xmlChar *)"att2");
                        if (att2 != NULL) {
                            tmp_att = (const char*)att2;
                            att2_i = (uint32_t)stoi(tmp_att);
                            xmlFree(att2);
                        }
                        att3 = xmlGetProp(node, (const xmlChar *)"att3");
                        if (att3 != NULL) {
                            tmp_att = (const char*)att3;
                            att3_i = (uint32_t)stoi(tmp_att);
                            xmlFree(att3);
                        }
                        tmp_data_header.attr[0] = (uint8_t)att0_i;
                        tmp_data_header.attr[1] = (uint8_t)att1_i;
                        tmp_data_header.attr[2] = (uint8_t)att2_i;
                        tmp_data_header.attr[3] = (uint8_t)att3_i;

                        DqeLutHeaderTable_entry.push_back(tmp_data_header);
                        DqeLutTable_entry.push_back(tmp_data);
                    }
                    node = node->next;
                }
                //DqeLutHeaderTable.push_back(DqeLutHeaderTable_entry);
                //DqeLutTable.push_back(DqeLutTable_entry);

                DqeLutMap.insert(make_pair(make_pair(tmpColorMode.modeId, tmpRenderIntent.intentId), DqeLutTable_entry));
                DqeLutHeaderMap.insert(make_pair(make_pair(tmpColorMode.modeId, tmpRenderIntent.intentId), DqeLutHeaderTable_entry));
                if (tmp_size > DqeLutSize)
                    DqeLutSize = tmp_size;
                DqeLutTable_entry.clear();
                DqeLutHeaderTable_entry.clear();
            }
next:
            cur = cur->next;

            if (mode_id != NULL)
                xmlFree(mode_id);
            if (mode_name != NULL)
                xmlFree(mode_name);
            if (gamut_id != NULL)
                xmlFree(gamut_id);
            if (gamut_name != NULL)
                xmlFree(gamut_name);
            if (intent_id != NULL)
                xmlFree(intent_id);
            if (intent_name != NULL)
                xmlFree(intent_name);
            if (subXml != NULL)
                xmlFree(subXml);
            if (subdoc != NULL)
                xmlFreeDoc(subdoc);
        }
        xmlFreeDoc(doc);
        return 0;
    }
    void clearDqeLut() {

        gHeaderBase.seq_num++;

        mode = -1;
        intent = -1;
        matrix_en = false;
        CFMatrix.clear();
    }
#if 0
    void printDump(void* parcel_dump, int size) {
        int i = 0;
        const char *parcel = (const char*)parcel_dump;
        ALOGD("%s: parcel(%lx), size(%d)", __func__, (unsigned long)parcel, size);
        if (size > 40) {
            for (i = 0; i < size; i+=40) {
                ALOGD("[%4x]\
                        %3x%3x%3x%3x%3x%3x%3x%3x%3x%3x\
                        %3x%3x%3x%3x%3x%3x%3x%3x%3x%3x\
                        %3x%3x%3x%3x%3x%3x%3x%3x%3x%3x\
                        %3x%3x%3x%3x%3x%3x%3x%3x%3x%3x", i,
                        parcel[i],parcel[i+1],parcel[i+2],parcel[i+3],parcel[i+4],parcel[i+5],parcel[i+6],parcel[i+7],parcel[i+8],parcel[i+9],
                        parcel[i+10],parcel[i+11],parcel[i+12],parcel[i+13],parcel[i+14],parcel[i+15],parcel[i+16],parcel[i+17],parcel[i+18],parcel[i+19],
                        parcel[i+20],parcel[i+21],parcel[i+22],parcel[i+23],parcel[i+24],parcel[i+25],parcel[i+26],parcel[i+27],parcel[i+28],parcel[i+29],
                        parcel[i+30],parcel[i+31],parcel[i+32],parcel[i+33],parcel[i+34],parcel[i+35],parcel[i+36],parcel[i+37],parcel[i+38],parcel[i+39]);
            }
        }
        int remainder = size % 40;
        if (remainder) {
            string tmpStr = "";
            char tempStr[32];
            std::snprintf(tempStr, sizeof(tempStr), "[%4x]", i);
            tmpStr += tempStr;
            for (; i < (i + remainder); i++) {
                snprintf(tempStr, sizeof(tempStr), "%3x", parcel[i]);
                tmpStr += tempStr;
            }
            ALOGD("%s", tmpStr.c_str());
        }
    }
#endif

    void printDqeLut(void *parcel_dump, uint16_t size) {
        const char *parcel = (const char *)parcel_dump;
        struct dqe_colormode_global_header *g_header = (struct dqe_colormode_global_header *)parcel;
        struct dqe_colormode_data_header * d_header = (struct dqe_colormode_data_header *)(parcel + sizeof(struct dqe_colormode_global_header));
        char *d_ = (char*)(parcel + sizeof(struct dqe_colormode_global_header) + sizeof(struct dqe_colormode_data_header));
        ALOGD("%s: parcel(%lx), size(%d)", __func__, (unsigned long)parcel, size);
        ALOGD("[header]");
        ALOGD("magic(0x%x)", g_header->magic);
        ALOGD("seq_num(%d)", g_header->seq_num);
        ALOGD("total_size(%d)", g_header->total_size);
        ALOGD("header_size(%d)", g_header->header_size);
        ALOGD("num_data(%d)", g_header->num_data);
        for (int i = 0; i < g_header->num_data; i++) {
            if (i != 0) {
                int total_size = d_header->total_size;
                d_header = (struct dqe_colormode_data_header *)((char*)d_header + total_size);
                d_ = (char*)d_ + total_size;
            }
            ALOGD("[data_header%d]", i);
            ALOGD("magic(0x%x)",         d_header->magic);
            ALOGD("id(%d)",              d_header->id);
            ALOGD("crc(0x%x)",           d_header->crc);
            ALOGD("total_size(%d)",      d_header->total_size);
            ALOGD("header_size(%d)",     d_header->header_size);
            ALOGD("attr0(%d)",        d_header->attr[0]);
            ALOGD("attr1(%d)",        d_header->attr[1]);
            ALOGD("attr2(%d)",        d_header->attr[2]);
            ALOGD("attr3(%d)",        d_header->attr[3]);
            ALOGD("[data%d]", i);
            ALOGD("%s", d_);
        }
    }

public:

    DisplayColorImplementation(uint32_t display_id) {
        int ret = 0;

        ret = initUtilityVariables();
        if (ret < 0)
            return;

        ret = buildupColorModeNodes(display_id);
        if (ret < 0)
            return;

        init_completed = true;
        ALOGD("%s init completed", __func__);
    }

    ~DisplayColorImplementation() {}

    vector<DisplayColorMode> getColorModes() {
        if (this->log_level > 1) {
            ALOGD("[supported color modes]");
            for (int i = 0; i < CMList.size(); i++)
                ALOGD("%s:[%d] modeId(%d), gamutId(%d), modeName(%s)", __func__,
                        i, CMList[i].modeId, CMList[i].gamutId, CMList[i].modeName.c_str());
        }
        return CMList;
    }
    int setColorMode(uint32_t mode) {
        this->mode = mode;
        this->intent = 0;
        if (this->log_level > 1)
            ALOGD("%s:mode(%d), intent(%d)", __func__, this->mode, this->intent);
        return 0;
    }
    vector<DisplayRenderIntent> getRenderIntents(uint32_t mode) {
        if (this->log_level > 1) {
            ALOGD("[supported render intents]");
            for (int i = 0; i < RIList[mode].size(); i++) {
                ALOGD("%s:[%d] intentId(%d), intentName(%s))", __func__,
                        i, RIList[mode][i].intentId, RIList[mode][i].intentName.c_str());
            }
        }
        return RIList[mode];
    }
    int setColorModeWithRenderIntent(uint32_t mode, uint32_t intent) {
        this->mode = mode;
        this->intent = intent;
        if (this->log_level > 1)
            ALOGD("%s:mode(%d), intent(%d)", __func__, this->mode, this->intent);
        return 0;
    }
    int setColorTransform(const float *matrix, uint32_t hint) {
        if (this->log_level > 1) {
            ALOGD("%s:matrix([%.2f,%.2f,%.2f,%.2f],[%.2f,%.2f,%.2f,%.2f],[%.2f,%.2f,%.2f,%.2f],[%.2f,%.2f,%.2f,%.2f]), hint(%d)",
                    __func__,
                        matrix[0], matrix[1], matrix[2], matrix[3],
                        matrix[4], matrix[5], matrix[6], matrix[7],
                        matrix[8], matrix[9], matrix[10], matrix[11],
                        matrix[12], matrix[13], matrix[14], matrix[15], hint);
        }
        CFMatrixHeader.magic = (uint8_t)DQE_COLORMODE_MAGIC;
        CFMatrixHeader.id = (uint8_t)DQE_COLORMODE_ID_GAMMA_MATRIX;
        CFMatrixHeader.header_size = (uint16_t)sizeof(struct dqe_colormode_data_header);
        CFMatrixHeader.attr[0] = (uint8_t)-1;
        CFMatrixHeader.attr[1] = (uint8_t)-1;
        CFMatrixHeader.attr[2] = (uint8_t)-1;
        CFMatrixHeader.attr[3] = (uint8_t)-1;

        int tmp_val;
        string tmp_str;
        string entry = "1,";
        hint = 0;
        for (int i = 0; i < 16; i++) {
            tmp_val = ((int)round(matrix[i] * 65536));
            tmp_str = to_string(tmp_val);
            entry += tmp_str;
            if (i != 15)
                entry += ",";
        }

        CFMatrix = entry;
        CFMatrixHeader.total_size = (uint16_t)(sizeof(struct dqe_colormode_data_header) + CFMatrix.size() + 1);
        CFMatrixHeader.crc = getCrc16((char*)CFMatrix.c_str(), (CFMatrix.size() + 1));

        matrix_en = true;
        return 0;
    }
    int getDqeLutSize() {return DqeLutSize;}
    int getDqeLut(void *parcel) {
        if (init_completed == false) {
            ALOGD("libdisplaycolor not initialized\n");
            return -1;
        }
        int offset = 0;
        int curDqeLutTotalSize = 0;
        int curDqeLutDataCnt = 0;
        bool has_matrix = false;
        vector<string> tmpData;
        vector<struct dqe_colormode_data_header> tmpDataHeader;
        if (mode != -1) {
            tmpData = DqeLutMap[make_pair(mode, intent)];
            tmpDataHeader = DqeLutHeaderMap[make_pair(mode, intent)];

            if (tmpData.size() != tmpDataHeader.size()) {
                ALOGD("number of data and header differ\n");
                return -1;
            }
            /* init : Header & Data */
            offset += sizeof(gHeaderBase);
            for (int i = 0; i < tmpData.size(); i++) {
                if (tmpDataHeader[i].id == DQE_COLORMODE_ID_GAMMA_MATRIX) {
                    if (matrix_en == true) {
                        has_matrix = true;
                        if (CFMatrix.size() != (CFMatrixHeader.total_size - CFMatrixHeader.header_size - 1)) {
                            ALOGD("size of actual data and size specified in header differ\n");
                            return -1;
                        }
                        memcpy((char*)parcel + offset, &CFMatrixHeader, sizeof(struct dqe_colormode_data_header));
                        offset += sizeof(struct dqe_colormode_data_header);
                        memcpy((char*)parcel + offset, CFMatrix.c_str(), CFMatrix.size() + 1);
                        offset += (CFMatrix.size() + 1);
                        continue;
                    }
                }
                if (tmpData[i].size() != (tmpDataHeader[i].total_size - tmpDataHeader[i].header_size - 1)) {
                    ALOGD("size of actual data and size specified in header differ\n");
                    return -1;
                }
                memcpy((char*)parcel + offset, &tmpDataHeader[i], sizeof(struct dqe_colormode_data_header));
                offset += sizeof(struct dqe_colormode_data_header);
                memcpy((char*)parcel + offset, tmpData[i].c_str(), tmpData[i].size() + 1);
                offset += (tmpData[i].size() + 1);
            }

            /* init : TF matrix */
            if (matrix_en == true && has_matrix != true) {
                memcpy((char*)parcel + offset, &CFMatrixHeader, sizeof(struct dqe_colormode_data_header));
                offset += sizeof(struct dqe_colormode_data_header);
                memcpy((char*)parcel + offset, CFMatrix.c_str(), CFMatrix.size() + 1);
                offset += (CFMatrix.size() + 1);
            }

            /* end : Global Header */
            curDqeLutTotalSize = offset;
            curDqeLutDataCnt = ((matrix_en == true) && (has_matrix != true)) ? (tmpData.size() + 1) : tmpData.size();
            offset = 0;
            gHeaderBase.total_size = curDqeLutTotalSize;
            gHeaderBase.num_data = curDqeLutDataCnt;
            memcpy((char*)parcel + offset, &gHeaderBase, sizeof(struct dqe_colormode_global_header));
        } else if (matrix_en != false) {
            /* init : TF matrix */
            int offset = sizeof(gHeaderBase);
            if (CFMatrix.size() != (CFMatrixHeader.total_size - CFMatrixHeader.header_size - 1)) {
                ALOGD("size of actual data and size specified in header differ\n");
                return -1;
            }
            memcpy((char*)parcel + offset, &CFMatrixHeader, sizeof(struct dqe_colormode_data_header));
            offset += sizeof(struct dqe_colormode_data_header);
            memcpy((char*)parcel + offset, CFMatrix.c_str(), CFMatrix.size() + 1);
            offset += (CFMatrix.size() + 1);

            /* end : Global Header */
            int curDqeLutTotalSize = offset;
            int curDqeLutDataCnt = 1;
            offset = 0;
            gHeaderBase.total_size = curDqeLutTotalSize;
            gHeaderBase.num_data = curDqeLutDataCnt;
            memcpy((char*)parcel + offset, &gHeaderBase, sizeof(struct dqe_colormode_global_header));
        } else {
            ALOGD("no set functions called prior to getDqeLut()\n");
            return -1;
        }

        if (log_level > 2)
#if 0
            printDump(parcel, DqeLutSize);
        else if (log_level > 1)
#endif
            printDqeLut(parcel, gHeaderBase.total_size);

        clearDqeLut();
        return 0;
    }
    void setLogLevel(int log_level) {this->log_level = log_level;}
};

IDisplayColor *IDisplayColor::createInstance(uint32_t display_id) {
    return new DisplayColorImplementation(display_id);
}
