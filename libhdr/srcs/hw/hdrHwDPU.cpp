#include "hdrHwInfo.h"
#include "hdrHwDPU.h"
#include "hdrUtil.h"
#include "libhdr_parcel_header.h"
#include <vector>

using namespace std;

static inline void parse_capa_exfunc(struct hdrExFunc *ex_func,
        xmlDocPtr xml_doc, xmlNodePtr xmlCapa)
{
    xmlNodePtr capa = xmlCapa;

    while (capa != NULL) {
        xmlChar *key = xmlNodeListGetString(xml_doc,
                capa->xmlChildrenNode, 1);
        vector<string> out;
        out.clear();

        if (!xmlStrcmp(capa->name, (const xmlChar*)"module-en")) {
            ex_func->modEn = string((char*)key);
        } else if (!xmlStrcmp(capa->name, (const xmlChar*)"submodules-en")) {
            split(string((char*)key), ',', out);
            ex_func->subModEn = out;
        } else if (!xmlStrcmp(capa->name, (const xmlChar*)"submodules")) {
            split(string((char*)key), ',', out);
            ex_func->subMod = out;
        }

        capa = capa->next;
        xmlFree(key);
    }
}

static inline void parse_capabilities(struct hdrModule &hdrMod,
        xmlDocPtr xml_doc, xmlNodePtr xmlCapas)
{
    xmlNodePtr capas = xmlCapas;
    struct hdrModuleCapabilities *capaVal = &hdrMod.hdrModCapa;

    while (capas != NULL) {
        string s = (char*)capas->name;
        if (capaVal->hdrFuncsMap.find(s)
                != capaVal->hdrFuncsMap.end()) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                    capas->xmlChildrenNode, 1);
            int val = stoi(string((char*)key));
            (*capaVal->hdrFuncsMap[s]) = (bool)val;
            xmlFree(key);
        } else if (capaVal->hdrBpcsMap.find(s)
                != capaVal->hdrBpcsMap.end()) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                    capas->xmlChildrenNode, 1);
            int val = stoi(string((char*)key));
            (*capaVal->hdrBpcsMap[s]) = (bool)val;
            xmlFree(key);
        } else if (capaVal->hdrExFuncsMap.find(s)
                != capaVal->hdrExFuncsMap.end()) {
            parse_capa_exfunc(capaVal->hdrExFuncsMap[s],
                    xml_doc, capas->xmlChildrenNode);
        }
        capas = capas->next;
    }
}

static inline void parse_submodulenode(struct hdrSubModule &hdrSubMod,
        xmlDocPtr xml_doc, xmlNodePtr xmlSubModNode)
{
    string hdrSubModIds[7] = { "num-nodes",
                "num-nodes-per-reg", "last-node-align",
                "bit-offsets", "masks",
                "reg-offset", "reg-num" };
    xmlNodePtr node = xmlSubModNode;
    while (node != NULL) {
        xmlChar *key = xmlNodeListGetString(xml_doc,
                    node->xmlChildrenNode, 1);
        vector<string> out;
        out.clear();
        if(!xmlStrcmp(node->name, (const xmlChar *)hdrSubModIds[0].c_str())) {
            hdrSubMod.numNodes = stoi(string((char*)key));
        } else if (!xmlStrcmp(node->name, (const xmlChar *)hdrSubModIds[1].c_str())) {
            hdrSubMod.numNodesPerReg = stoi(string((char*)key));
        } else if (!xmlStrcmp(node->name, (const xmlChar *)hdrSubModIds[2].c_str())) {
            hdrSubMod.lastNodeAlign = stoi(string((char*)key));
        } else if (!xmlStrcmp(node->name, (const xmlChar *)hdrSubModIds[3].c_str())) {
            split(string((char*)key), ',', out);
            for (auto &s : out)
                hdrSubMod.bitOffsets.push_back(stoi(s));
        } else if (!xmlStrcmp(node->name, (const xmlChar *)hdrSubModIds[4].c_str())) {
            split(string((char*)key), ',', out);
            for (auto &s : out)
                hdrSubMod.masks.push_back(strToHex(s));
        } else if (!xmlStrcmp(node->name, (const xmlChar *)hdrSubModIds[5].c_str())) {
            hdrSubMod.regOffset = strToHex(string((char*)key));
        } else if (!xmlStrcmp(node->name, (const xmlChar *)hdrSubModIds[6].c_str())) {
            hdrSubMod.regNum = stoi(string((char*)key));
        }
        node = node->next;
        xmlFree(key);
    }
}

static inline void parse_submodule(struct hdrModule &hdrMod,
        xmlDocPtr xml_doc, xmlNodePtr xmlSubMod)
{
    xmlNodePtr sub = xmlSubMod;
    hdrSubModules *submods = &hdrMod.hdrSubMods;

    while (sub != NULL) {
        struct hdrSubModule hdrSubMod;
        hdrSubMod.init();
        if (!xmlStrcmp(sub->name, (const xmlChar *)"submodule")) {
            xmlChar *group_id = xmlGetProp(sub, (const xmlChar *)"group-id");
            if (group_id != NULL)
                hdrSubMod.groupId = stoi(string((char*)group_id));

            xmlChar *subname = xmlGetProp(sub, (const xmlChar *)"name");

            parse_submodulenode(hdrSubMod,
                    xml_doc, sub->xmlChildrenNode);

            submods->nameToHdrSubMod.insert(make_pair(string((char*)subname), hdrSubMod));

            xmlFree(subname);
            xmlFree(group_id);
        }
        sub = sub->next;
    }
}

static inline void parse_mod_subinfo(struct hdrModule &hdrMod,
        xmlDocPtr xml_doc, xmlNodePtr xmlSubInfo)
{
    xmlNodePtr subInfo = xmlSubInfo;

    while (subInfo != NULL) {
        if (!xmlStrcmp(subInfo->name, (const xmlChar *)"capabilities")) {
            parse_capabilities(hdrMod, xml_doc,subInfo->xmlChildrenNode);
        } else if (!xmlStrcmp(subInfo->name, (const xmlChar *)"submodule_info")) {
            parse_submodule(hdrMod, xml_doc, subInfo->xmlChildrenNode);
        }
        subInfo = subInfo->next;
    }
}

void hdrHwDPU::parse_module(
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module)
{
    xmlNodePtr module = xml_module;
    xmlChar *module_id;

    int modId = 0;
    while (module != NULL) {
        struct hdrModule hdrMod;
        vector<string> hdrModIds;
        if (xmlStrcmp(module->name, (const xmlChar *)"module")) {
            module = module->next;
            continue;
        }

        hdrMod.init();
        hdrModIds.clear();

        module_id = xmlGetProp(module, (const xmlChar *)"id");
        split(string((char*)module_id), ',', hdrModIds);
        xmlFree(module_id);

        parse_mod_subinfo(hdrMod, xml_doc, module->xmlChildrenNode);

        hdrMod.id = modId++;
        for (int i = 0; i < hdrModIds.size(); i++) {
            int hdrModId = stoi(hdrModIds[i]);
            layerToHdrMod.insert(make_pair(hdrModId, hdrMod));
        }

        module = module->next;
    }
}

void hdrHwDPU::parse(std::string &hdrInfoId, std::string &filename)
{
    xmlDocPtr doc;
    xmlNodePtr root;
    xmlChar *root_id;

    doc = xmlParseFile(filename.c_str());
    if (doc == NULL) {
        ALOGE("no document available(%s)", filename.c_str());
        goto ret;
    }

    root = xmlDocGetRootElement(doc);
    if (root == NULL) {
        ALOGE("empty document");
        goto free_doc;
    }

    if (xmlStrcmp(root->name, (const xmlChar *)hdrInfoId.c_str())) {
        ALOGE("document of the wrong type, root node != %s", hdrInfoId.c_str());
        goto free_root;
    }

    root_id = xmlGetProp(root, (const xmlChar *)"name");
    if (xmlStrcmp(root_id, (const xmlChar *)hdrHwId.c_str())) {
        ALOGE("document with unintented hw id(intended:%s, parsed:%s)",
                hdrHwId.c_str(), root_id);
        goto free_root_id;
    }

    parse_module(doc, root->xmlChildrenNode);

free_root_id:
    xmlFree(root_id);
free_root:
    //xmlFree(root);
free_doc:
    xmlFreeDoc(doc);
ret:
    //dump();
    return;
}

int hdrHwDPU::getSubModNodeNum(std::string &subModName, int layer)
{
    hdrModule *mMod = &layerToHdrMod[layer];
    hdrSubModule *mSubMod = &mMod->hdrSubMods.nameToHdrSubMod[subModName];
    return mSubMod->numNodes;
}

int hdrHwDPU::pack(std::string &subModName, int layer,
            std::vector<int> &in, struct hdr_dat_node &out)
{
    int ret = 0;
    hdrModule *mMod = &layerToHdrMod[layer];
    hdrSubModule *mSubMod = &mMod->hdrSubMods.nameToHdrSubMod[subModName];
    ret = mSubMod->pack(in, out);
    if (ret != 0) {
        ALOGE("> pack() failed (subModName:%s, layer:%d)",
                subModName.c_str(), layer);
        return ret;
    }

    return ret;
}

void hdrHwDPU::dump(void)
{
    ALOGD("Layer To Module Map[]");
    for (auto &l : layerToHdrMod) {
        ALOGD("[Layer id(%d) -> Module id(%d)]", l.first, l.second.id);
        l.second.dump();
    }
    return;
}

int hdrHwDPU::getLayerNum(void)
{
    return layerToHdrMod.size();
}

bool hdrHwDPU::hasMod(int layer)
{
    return layerToHdrMod.find(layer) != layerToHdrMod.end();
}

bool hdrHwDPU::hasSubMod(int layer, std::string &subModName)
{
    if (hasMod(layer)) {
        hdrModule *mMod = &layerToHdrMod[layer];
        return mMod->hdrSubMods.nameToHdrSubMod.find(subModName) != mMod->hdrSubMods.nameToHdrSubMod.end();
    } else {
        return false;
    }
}

int hdrHwDPU::getHdrCoefSize(void)
{
    if (coefSize == -1) {
        for (auto &layer: layerToHdrMod) {
            int size = sizeof(struct hdr_coef_header);
            hdrModule *mMod = &layer.second;
            for (auto iter = mMod->hdrSubMods.nameToHdrSubMod.begin(); iter != mMod->hdrSubMods.nameToHdrSubMod.end(); iter++)
                size += (sizeof(hdr_lut_header) + (sizeof(int) * ((int)(iter->second.numNodes / iter->second.numNodesPerReg) + 1)));
            coefSize = std::max(size, coefSize);
        }
    }
    return coefSize * 2;
}
