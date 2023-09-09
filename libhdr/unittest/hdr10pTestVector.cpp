#include "hdr10pTestVector.h"

using namespace std;

void hdr10pTestVector::parse_hdr10Node(
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

void hdr10pTestVector::parse_hdr10Mod(
        int hw_id,
        int module_id,
        struct hdr10Module &hdr10_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module)
{
    xmlNodePtr subInfo = xml_module;
    while (subInfo != NULL) {
        if (!xmlStrcmp(subInfo->name, (const xmlChar *)"hdr10p-node")) {
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

void hdr10pTestVector::parse_hdr10Mods(
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

void hdr10pTestVector::__parse__(int hw_id)
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

    if (xmlStrcmp(root->name, (const xmlChar *)"HDR10P_LUT")) {
        ALOGE("document of the wrong type, root node != HDR10P_LUT");
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

void hdr10pTestVector::parse(std::vector<struct supportedHdrHw> *list)
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

void hdr10pTestVector::init(hdrHwInfo *hwInfo)
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

#define META_JSON_2094_100K_PERCENTILE_DIVIDE    10.0
float hdr10pTestVector::getSrcMaxLuminance(ExynosHdrDynamicInfo *dyn_meta)
{
	unsigned int order = dyn_meta->data.num_maxrgb_percentiles[0];
	double lumNorm = META_JSON_2094_100K_PERCENTILE_DIVIDE;
	float maxscl[3];
	unsigned int percentages[15];
	double percentiles[15];
	float maxLuminance = 0;

        for (unsigned int i = 0; i < 3; i++)
		maxscl[i] = dyn_meta->data.maxscl[0][i] / lumNorm;
        for (unsigned int i = 0; i < order; i++) {
		percentiles[i] = dyn_meta->data.maxrgb_percentiles[0][i] / lumNorm;
		percentages[i] = dyn_meta->data.maxrgb_percentages[0][i];
	}

	for (unsigned int i = 0; i < 3; i++)
		maxLuminance = std::max(maxLuminance, maxscl[i]);

	if ((order > 0) && (percentages[order - 1] == 99))
		maxLuminance = std::min(percentiles[order - 1], static_cast<double> (maxLuminance));

	return maxLuminance;
}

std::unordered_map<int,struct hdr_dat_node*>& hdr10pTestVector::getTestVector(
        int hw_id, int layer_index, ExynosHdrStaticInfo *s_meta, ExynosHdrDynamicInfo *d_meta)
{
    struct hdr10Module *module = &layerToHdr10Mod[layer_index];
    int s_max_luminance = (s_meta->sType1.mMaxDisplayLuminance / 10000);
    int d_max_luminance = getSrcMaxLuminance(d_meta);
    int mastering_luminance = (s_max_luminance > d_max_luminance) ? s_max_luminance : d_max_luminance;

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

void setDynamicMeta_Random(ExynosHdrDynamicInfo *dyn_meta)
{
    for (int i = 0; i < sizeof(ExynosHdrDynamicInfo); i++) {
        srand(time(NULL) + i);
        *((unsigned char*)dyn_meta) = (rand() % 255);
    }
    dyn_meta->data.num_windows = 1;
    dyn_meta->data.num_rows_targeted_system_display_actual_peak_luminance = 1;
    dyn_meta->data.num_cols_targeted_system_display_actual_peak_luminance = 1;
    dyn_meta->data.num_maxrgb_percentiles[0] = 10;
    dyn_meta->data.num_rows_mastering_display_actual_peak_luminance = 1;
    dyn_meta->data.num_cols_mastering_display_actual_peak_luminance = 1;
    dyn_meta->data.tone_mapping.num_bezier_curve_anchors[0] = 9;
}

void hdr10pTestVector::setDynamicMeta(ExynosHdrDynamicInfo *dyn_meta, int index)
{
    if (index < 0) // randomly
        setDynamicMeta_Random(dyn_meta);
    else
        memcpy(dyn_meta, &this->d_meta[index], sizeof(ExynosHdrDynamicInfo));
}
