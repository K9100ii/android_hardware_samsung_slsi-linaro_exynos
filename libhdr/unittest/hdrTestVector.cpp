#include "hdrTestVector.h"

using namespace std;

void hdrTestVector::parse_hdr10Node(
        int hw_id,
        int module_id,
        struct hdr10Node __attribute__((unused)) &hdr10_node,
        xmlDocPtr __attribute__((unused)) xml_doc,
        xmlNodePtr xml_hdr10node)
{
    IHdrHw *IHw = hwInfo->getIf(hw_id);
    xmlNodePtr hdr10node = xml_hdr10node;
    while (hdr10node != NULL) {
        string subModName = string((char*)hdr10node->name);
        if (IHw->hasSubMod(module_id, subModName)) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                hdr10node->xmlChildrenNode, 1);
            std::vector<int> out_vec;
            split(string((char*)key), ',', out_vec);
            xmlFree(key);

            struct hdr_dat_node tmp;
            IHw->pack(subModName, module_id, out_vec, tmp);
            
            hdr10_node.coef_packed.push_back(tmp);
        }
        hdr10node = hdr10node->next;
    }
}

void hdrTestVector::parse_hdr10Mod(
        int hw_id,
        int module_id,
        struct hdr10Module &hdr10_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module)
{
    xmlNodePtr subInfo = xml_module;
    while (subInfo != NULL) {
        if (!xmlStrcmp(subInfo->name, (const xmlChar *)"hdr10-node")) {
            struct hdr10Node tmpNode;
            xmlChar *max_lum = xmlGetProp(subInfo, (const xmlChar *)"max-luminance");
            tmpNode.max_luminance = stoi(string((char*)max_lum));
            xmlFree(max_lum);

            parse_hdr10Node(hw_id, module_id, tmpNode, xml_doc, subInfo->xmlChildrenNode);

            hdr10_module.hdr10NodeTable.push_back(tmpNode);
        }
        subInfo = subInfo->next;
    }
}

void hdrTestVector::parse_hdr10Mods(
        int hw_id,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module)
{
    xmlNodePtr module = xml_module;
    xmlChar *module_id;
    std::vector<int> hdr10ModIds;
    while (module != NULL) {
        struct hdr10Module hdr10Mod;
        if (xmlStrcmp(module->name, (const xmlChar *)"hdr-module")) {
            module = module->next;
            continue;
        }
        module_id = xmlGetProp(module, (const xmlChar *)"layer");
        split(string((char*)module_id), ',', hdr10ModIds);
        xmlFree(module_id);

        for (auto &modId : hdr10ModIds) {
            parse_hdr10Mod(hw_id, modId, hdr10Mod, xml_doc, module->xmlChildrenNode);
            hdr10Mod.sort_ascending();
            layerToHdr10Mod.insert(make_pair(modId, hdr10Mod));
        }

        module = module->next;
    }
}

void hdrTestVector::__parse__(int hw_id)
{
    xmlDocPtr doc;
    xmlNodePtr root;

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

    if (xmlStrcmp(root->name, (const xmlChar *)"HDR10_LUT")) {
        ALOGE("document of the wrong type, root node != HDR10_LUT");
        goto free_root;
    }

    parse_hdr10Mods(hw_id, doc, root->xmlChildrenNode);

free_root:
    //xmlFree(root);
free_doc:
    xmlFreeDoc(doc);
ret:
    return;
}

void hdrTestVector::parse(std::vector<struct supportedHdrHw> *list)
{
    for (auto &item : *list) {
        switch (item.id) {
            case HDR_HW_DPU:
                __parse__(HDR_HW_DPU);
                break;
            default:
                ALOGE("hw(%d) not supported", item.id);
                break;
        }
    }
}

void hdrTestVector::init(hdrHwInfo *hwInfo)
{
    this->hwInfo = hwInfo;
    parse(hwInfo->getListHdrHw());

    auto list = hwInfo->getListHdrHw();
    for (int i = 0; i < list->size(); i++) {
        int hw_id = (*list)[i].id;
        group_list[hw_id].resize((*list)[i].IHw->getLayerNum());
        for (int j = 0; j < (*list)[i].IHw->getLayerNum(); j++) {
            std::unordered_map<int, struct hdr_dat_node> tmp;
            tmp.clear();
            group_list[hw_id][j] = tmp;
        }
    }
}

std::unordered_map<int,struct hdr_dat_node*>& hdrTestVector::getTestVector(
        int hw_id, int layer_index, ExynosHdrStaticInfo *s_meta)
{
    struct hdr10Module *module = &layerToHdr10Mod[layer_index];
    int mastering_luminance = (s_meta->sType1.mMaxDisplayLuminance / 10000);

    group_list[hw_id][layer_index].clear();

    for (auto iter = module->hdr10NodeTable.begin();
            iter != module->hdr10NodeTable.end(); iter++) {
        if (mastering_luminance <= iter->max_luminance) {
            for (auto __iter__ = iter->coef_packed.begin();
                    __iter__ != iter->coef_packed.end(); __iter__++) {
                struct hdr_dat_node* dat = &(*__iter__);
                if (dat->group_id < 0)
                    hdrTV_map[dat->header.byte_offset] = dat;
                else {
                    if (group_list[hw_id][layer_index].find(dat->group_id) != group_list[hw_id][layer_index].end())
                        group_list[hw_id][layer_index][dat->group_id].merge(dat);
                    else {
                        struct hdr_dat_node tmp; 
                        tmp = (*dat);
                        group_list[hw_id][layer_index][dat->group_id] = tmp;
                    }
                }
            }
            break;
        }
    }
    for (auto iter = group_list[hw_id][layer_index].begin(); iter != group_list[hw_id][layer_index].end(); iter++)
        hdrTV_map[iter->second.header.byte_offset] = &iter->second;

    return hdrTV_map;
}
