#include "wcgTestVector.h"

using namespace std;

void wcgTestVector::parse_inoutGamut(
        int hw_id,
        int module_id,
        struct wcgModule &wcg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_in)
{
    IHdrHw *IHw = hwInfo->getIf(hw_id);
    xmlNodePtr in = xml_in;
    while (in != NULL) {
        if (!xmlStrcmp(in->name, (const xmlChar *)"in")) {
            xmlChar* in_gamut = xmlGetProp(in, (const xmlChar *)"gamut");
            int in_gamut_id = stStrMap[string((char*)in_gamut)];
            //ALOGD("in gamut(%s)", (char*)in_gamut);
            xmlFree(in_gamut);

            xmlNodePtr out = in->xmlChildrenNode;
            while (out != NULL) {
                if (!xmlStrcmp(out->name, (const xmlChar *)"out")) {
                    xmlChar* out_gamut = xmlGetProp(out, (const xmlChar *)"gamut");
                    int out_gamut_id = stStrMap[string((char*)out_gamut)];
                    //ALOGD("->out gamut(%s)", (char*)out_gamut);
                    xmlFree(out_gamut);

                    xmlNodePtr subMod = out->xmlChildrenNode;
                    while (subMod != NULL) {
                        string subModName = (char*)subMod->name;
                        if (IHw->hasSubMod(module_id, subModName)) {
                            xmlChar *key = xmlNodeListGetString(xml_doc,
                                subMod->xmlChildrenNode, 1);
                            std::vector<int> out_vec;
                            split(string((char*)key), ',', out_vec);
                            xmlFree(key);

                            struct gm tmp;
                            tmp.gmCoef = out_vec;
                            IHw->pack(subModName, module_id, out_vec, tmp.gmCoef_packed);
                            wcg_module.gmTable[in_gamut_id].out[out_gamut_id].data.insert(make_pair(subModName, tmp));
                            //wcg_module.gmTable[in_gamut_id].out[out_gamut_id].data[subModName].dump();
                        }
                        subMod = subMod->next;
                    }
                }
                out = out->next;
            }
        }
        in = in->next;
    }
}

void wcgTestVector::parse_outTransfer(
        int hw_id,
        int module_id,
        struct wcgModule &wcg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_out)
{
    IHdrHw *IHw = hwInfo->getIf(hw_id);
    xmlNodePtr out = xml_out;
    while (out != NULL) {
        if (!xmlStrcmp(out->name, (const xmlChar *)"transfer")) {
            xmlChar* function = xmlGetProp(out, (const xmlChar *)"function");
            if (tfStrMap.find(string((char*)function)) != tfStrMap.end()) {
                int id = tfStrMap[string((char*)function)];
                if (id >= wcg_module.oetfTable.capacity())
                    ALOGD("%s: transfer id exceeded its limit(function(%s), id(%d), limit(%lu)",
                            __func__, (char*)function, id, (unsigned long)wcg_module.oetfTable.capacity());
                else {
                    xmlNodePtr subMod = out->xmlChildrenNode;
                    while (subMod != NULL) {
                        string subModName = (char*)subMod->name;
                        if (IHw->hasSubMod(module_id, subModName)) {
                            //ALOGD("function(%s), subModName(%s)", (char*)function, subModName.c_str());
                            xmlChar *key = xmlNodeListGetString(xml_doc,
                                subMod->xmlChildrenNode, 1);
                            std::vector<int> out_vec;
                            split(string((char*)key), ',', out_vec);
                            xmlFree(key);

                            struct oetf tmp;
                            tmp.oetfCoef = out_vec;
                            IHw->pack(subModName, module_id, out_vec, tmp.oetfCoef_packed);
                            wcg_module.oetfTable[id].data.insert(make_pair(subModName, tmp));
                            //wcg_module.oetfTable[id].data[subModName].dump();
                        }
                        subMod = subMod->next;
                    }
                }
            }
            xmlFree(function);
        }
        out = out->next;
    }
}

void wcgTestVector::parse_inTransfer(
        int hw_id,
        int module_id,
        struct wcgModule &wcg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_in)
{
    IHdrHw *IHw = hwInfo->getIf(hw_id);
    xmlNodePtr in = xml_in;
    while (in != NULL) {
        if (!xmlStrcmp(in->name, (const xmlChar *)"transfer")) {
            xmlChar* function = xmlGetProp(in, (const xmlChar *)"function");
            if (tfStrMap.find(string((char*)function)) != tfStrMap.end()) {
                int id = tfStrMap[string((char*)function)];
                if (id >= wcg_module.eotfTable.capacity())
                    ALOGD("%s: transfer id exceeded its limit(function(%s), id(%d), limit(%lu)",
                            __func__, (char*)function, id, (unsigned long)wcg_module.eotfTable.capacity());
                else {
                    xmlNodePtr subMod = in->xmlChildrenNode;
                    while (subMod != NULL) {
                        string subModName = (char*)subMod->name;
                        if (IHw->hasSubMod(module_id, subModName)) {
                            //ALOGD("function(%s), subModName(%s)", (char*)function, subModName.c_str());
                            xmlChar *key = xmlNodeListGetString(xml_doc,
                                subMod->xmlChildrenNode, 1);
                            std::vector<int> in_vec;
                            split(string((char*)key), ',', in_vec);
                            xmlFree(key);

                            struct eotf tmp;
                            tmp.eotfCoef = in_vec;
                            IHw->pack(subModName, module_id, in_vec, tmp.eotfCoef_packed);
                            wcg_module.eotfTable[id].data.insert(make_pair(subModName, tmp));
                            //wcg_module.eotfTable[id].data[subModName].dump();
                        }
                        subMod = subMod->next;
                    }
                }
            }
            xmlFree(function);
        }
        in = in->next;
    }
}

void wcgTestVector::parse_inoutTransfer(
        int hw_id,
        int module_id,
        struct wcgModule &wcg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_option)
{
    xmlNodePtr option = xml_option;
    while (option != NULL) {
        if (!xmlStrcmp(option->name, (const xmlChar *)"in"))
            parse_inTransfer(hw_id, module_id, wcg_module,
                    xml_doc, option->xmlChildrenNode);
        else if (!xmlStrcmp(option->name, (const xmlChar *)"out"))
            parse_outTransfer(hw_id, module_id, wcg_module,
                    xml_doc, option->xmlChildrenNode);
        option = option->next;
    }
}

void wcgTestVector::parse_inoutDataspace(
        int hw_id,
        int module_id,
        struct wcgModule &wcg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_option)
{
    xmlNodePtr option = xml_option;
    IHdrHw *IHw = hwInfo->getIf(hw_id);
    while (option != NULL) {
        if (!xmlStrcmp(option->name, (const xmlChar *)"eq")) {
            xmlNodePtr eqSubMod = option->xmlChildrenNode;
            while (eqSubMod != NULL) {
                string subModName = (char*)eqSubMod->name;
                if (IHw->hasSubMod(module_id, subModName)) {
                    xmlChar *key = xmlNodeListGetString(xml_doc,
                        eqSubMod->xmlChildrenNode, 1);
                    std::vector<int> in;
                    split(string((char*)key), ',', in);
                    xmlFree(key);

                    struct modEn tmp;
                    tmp.modEnCoef = (bool)in[0];
                    IHw->pack(subModName, module_id, in, tmp.modEnCoef_packed);
                    //tmp.modEnCoef_packed.dump();
                    wcg_module.modEnTable[0].data.insert(make_pair(subModName, tmp));
                }
                eqSubMod = eqSubMod->next;
            }
        } else if (!xmlStrcmp(option->name, (const xmlChar *)"neq")) {
            xmlNodePtr neqSubMod = option->xmlChildrenNode;
            while (neqSubMod != NULL) {
                string subModName = (char*)neqSubMod->name;
                if (IHw->hasSubMod(module_id, subModName)) {
                    xmlChar *key = xmlNodeListGetString(xml_doc,
                        neqSubMod->xmlChildrenNode, 1);
                    std::vector<int> in;
                    split(string((char*)key), ',', in);
                    xmlFree(key);

                    struct modEn tmp;
                    tmp.modEnCoef = (bool)in[0];
                    IHw->pack(subModName, module_id, in, tmp.modEnCoef_packed);
                    //tmp.modEnCoef_packed.dump();
                    wcg_module.modEnTable[1].data.insert(make_pair(subModName, tmp));
                }
                neqSubMod = neqSubMod->next;
            }
        }
        option = option->next;
    }
}

void wcgTestVector::parse_wcgMod(
        int hw_id,
        int module_id,
        struct wcgModule &wcg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module)
{
    xmlNodePtr subInfo = xml_module;
    while (subInfo != NULL) {
        if (!xmlStrcmp(subInfo->name, (const xmlChar *)"inout-dataspace"))
            parse_inoutDataspace(hw_id, module_id, wcg_module, xml_doc, subInfo->xmlChildrenNode);
        else if (!xmlStrcmp(subInfo->name, (const xmlChar *)"inout-transfer"))
            parse_inoutTransfer(hw_id, module_id, wcg_module, xml_doc, subInfo->xmlChildrenNode);
        else if (!xmlStrcmp(subInfo->name, (const xmlChar *)"inout-gamut"))
            parse_inoutGamut(hw_id, module_id, wcg_module, xml_doc, subInfo->xmlChildrenNode);
        subInfo = subInfo->next;
    }
}

void wcgTestVector::parse_wcgMods(
        int hw_id,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module)
{
    xmlNodePtr module = xml_module;
    xmlChar *module_id;
    vector<int> wcgModIds;
    while (module != NULL) {
        struct wcgModule wcgMod;
        if (xmlStrcmp(module->name, (const xmlChar *)"wcg-module")) {
            module = module->next;
            continue;
        }
        module_id = xmlGetProp(module, (const xmlChar *)"layer");
        split(string((char*)module_id), ',', wcgModIds);
        xmlFree(module_id);

        for (auto &modId : wcgModIds) {
            parse_wcgMod(hw_id, modId, wcgMod, xml_doc, module->xmlChildrenNode);

            layerToWcgMod.insert(make_pair(modId, wcgMod));
        }

        module = module->next;
    }
}

void wcgTestVector::__parse__(int hw_id)
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

    if (xmlStrcmp(root->name, (const xmlChar *)"WCG")) {
        ALOGE("document of the wrong type, root node != WCG");
        goto free_root;
    }

    parse_wcgMods(hw_id, doc, root->xmlChildrenNode);

free_root:
    //xmlFree(root);
free_doc:
    xmlFreeDoc(doc);
ret:
    return;
}

void wcgTestVector::parse(vector<struct supportedHdrHw> *list)
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

void wcgTestVector::init(hdrHwInfo *hwInfo)
{
    tfStrMap.clear();
    for (auto &tf : transferTable)
        tfStrMap[tf.str] = tf.id;
    stStrMap.clear();
    for (auto &st : standardTable)
        stStrMap[st.str] = st.id;

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

std::unordered_map<int,struct hdr_dat_node*>& wcgTestVector::getTestVector(
        int hw_id, int layer_index,
        int in_dataspace, int out_dataspace) {
    int input_transfer = (in_dataspace & HAL_DATASPACE_TRANSFER_MASK) >> HAL_DATASPACE_TRANSFER_SHIFT;
    int input_gamut = (in_dataspace & HAL_DATASPACE_STANDARD_MASK) >> HAL_DATASPACE_STANDARD_SHIFT;
    int output_transfer = (out_dataspace & HAL_DATASPACE_TRANSFER_MASK) >> HAL_DATASPACE_TRANSFER_SHIFT;
    int output_gamut = (out_dataspace & HAL_DATASPACE_STANDARD_MASK) >> HAL_DATASPACE_STANDARD_SHIFT;
    struct wcgModule *module = &layerToWcgMod[layer_index];

    wcgTV_map.clear();
    group_list[hw_id][layer_index].clear();

    if (input_transfer == output_transfer && input_gamut == output_gamut) {
        for (auto iter = module->modEnTable[0].data.begin();
                iter != module->modEnTable[0].data.end(); iter++) {
            struct hdr_dat_node* dat = &iter->second.modEnCoef_packed;
            if (dat->group_id < 0)
                wcgTV_map[dat->header.byte_offset] = dat;
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
    } else {
        for (auto iter = module->modEnTable[1].data.begin();
                iter != module->modEnTable[1].data.end(); iter++) {
            struct hdr_dat_node* dat = &iter->second.modEnCoef_packed;
            if (dat->group_id < 0)
                wcgTV_map[dat->header.byte_offset] = dat;
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

        for (auto iter = module->eotfTable[input_transfer].data.begin();
                iter != module->eotfTable[input_transfer].data.end(); iter++) {
            struct hdr_dat_node* dat = &iter->second.eotfCoef_packed;
            if (dat->group_id < 0)
                wcgTV_map[dat->header.byte_offset] = dat;
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

        for (auto iter = module->oetfTable[output_transfer].data.begin();
                iter != module->oetfTable[output_transfer].data.end(); iter++) {
            struct hdr_dat_node* dat = &iter->second.oetfCoef_packed;
            if (dat->group_id < 0)
                wcgTV_map[dat->header.byte_offset] = dat;
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

        for (auto iter = module->gmTable[input_gamut].out[output_gamut].data.begin();
                iter != module->gmTable[input_gamut].out[output_gamut].data.end(); iter++) {
            struct hdr_dat_node* dat = &iter->second.gmCoef_packed;
            if (dat->group_id < 0)
                wcgTV_map[dat->header.byte_offset] = dat;
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
    }

    for (auto iter = group_list[hw_id][layer_index].begin();
            iter != group_list[hw_id][layer_index].end(); iter++)
        wcgTV_map[iter->second.header.byte_offset] = &iter->second;
    return wcgTV_map;
}
