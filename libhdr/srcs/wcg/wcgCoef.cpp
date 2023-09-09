#include "wcgCoef.h"
#include "hdrContext.h"
#include <unistd.h>

using namespace std;

void wcgCoef::parse_inoutGamut(
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

void wcgCoef::parse_outTransfer(
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

void wcgCoef::parse_inTransfer(
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

void wcgCoef::parse_inoutTransfer(
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

void wcgCoef::parse_inoutDataspace(
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

void wcgCoef::parse_inoutCustom(
        struct wcgModule &wcg_module,
        xmlNodePtr xml_in)
{
    xmlNodePtr in = xml_in;
    while (in != NULL) {
        if (!xmlStrcmp(in->name, (const xmlChar *)"in")) {
            int in_gamut_id = stStrMap["UNSPECIFIED"];
            int in_transfer_id = tfStrMap["UNSPECIFIED"];
            int in_dataspace = HAL_DATASPACE_UNKNOWN;

            xmlChar* in_gamut = xmlGetProp(in, (const xmlChar *)"gamut");
            if (in_gamut) {
                in_gamut_id = stStrMap[string((char*)in_gamut)];
                xmlFree(in_gamut);
            }
            xmlChar* in_transfer = xmlGetProp(in, (const xmlChar *)"transfer");
            if (in_transfer) {
                in_transfer_id = tfStrMap[string((char*)in_transfer)];
                xmlFree(in_transfer);
            }
            in_dataspace = (in_gamut_id << HAL_DATASPACE_STANDARD_SHIFT) |
                            (in_transfer_id << HAL_DATASPACE_TRANSFER_SHIFT);
            xmlNodePtr out = in->xmlChildrenNode;
            while (out != NULL && in_dataspace != HAL_DATASPACE_UNKNOWN) {
                if (!xmlStrcmp(out->name, (const xmlChar *)"out")) {
                    int out_gamut_id = stStrMap["UNSPECIFIED"];
                    int out_transfer_id = tfStrMap["UNSPECIFIED"];
                    int out_capa_id = capStrMap["UNSPECIFIED"];
                    int out_dataspace = HAL_DATASPACE_UNKNOWN;

                    xmlChar* out_gamut = xmlGetProp(out, (const xmlChar *)"gamut");
                    if (out_gamut) {
                        out_gamut_id = stStrMap[string((char*)out_gamut)];
                        xmlFree(out_gamut);
                    }
                    xmlChar* out_transfer = xmlGetProp(out, (const xmlChar *)"transfer");
                    if (out_transfer) {
                        out_transfer_id = tfStrMap[string((char*)out_transfer)];
                        xmlFree(out_transfer);
                    }
                    xmlChar* out_capa = xmlGetProp(out, (const xmlChar *)"capa");
                    if (out_capa) {
                        out_capa_id = capStrMap[string((char*)out_capa)];
                        xmlFree(out_capa);
                    }
                    out_dataspace = (out_gamut_id << HAL_DATASPACE_STANDARD_SHIFT) |
                                    (out_transfer_id << HAL_DATASPACE_TRANSFER_SHIFT);
                    wcg_module.customTable[in_dataspace] = {out_dataspace, out_capa_id};
                }
                out = out->next;
            }
        }
        in = in->next;
    }
}

void wcgCoef::parse_wcgMod(
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
        else if (!xmlStrcmp(subInfo->name, (const xmlChar *)"inout-custom"))
            parse_inoutCustom(wcg_module, subInfo->xmlChildrenNode);
        subInfo = subInfo->next;
    }
}

void wcgCoef::parse_wcgMods(
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

void wcgCoef::__parse__(int hw_id, struct hdrContext *ctx)
{
    xmlDocPtr doc;
    xmlNodePtr root;

    std::string fn_default = filename + (std::string)".xml";
    std::string fn_target = filename + ctx->target_name;

    doc = xmlParseFile(fn_target.c_str());
    if (doc == NULL) {
        ALOGD("no document or can not parse the document(%s)",
                fn_target.c_str());
        doc = xmlParseFile(fn_default.c_str());
        if (doc == NULL) {
            ALOGD("no document or can not parse the default document(%s)",
                fn_default.c_str());
            goto ret;
        }
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

void wcgCoef::parse(vector<struct supportedHdrHw> *list, struct hdrContext *ctx)
{
    for (auto &item : *list) {
        switch (item.id) {
            case HDR_HW_DPU:
                __parse__(HDR_HW_DPU, ctx);
                break;
            default:
                ALOGE("hw(%d) not supported", item.id);
                break;
        }
    }
}

void wcgCoef::init(hdrHwInfo *hwInfo, struct hdrContext *ctx)
{
    capStrMap = mapStringToCapa();
    tfStrMap.clear();
    for (auto &tf : transferTable)
        tfStrMap[tf.str] = tf.id;
    stStrMap.clear();
    for (auto &st : standardTable)
        stStrMap[st.str] = st.id;

    this->hwInfo = hwInfo;
    parse(hwInfo->getListHdrHw(), ctx);
    //this->dump();
}

int wcgCoef::coefBuildup(int layer_index, struct hdrContext *ctx)
{
    int ret = HDR_ERR_NO;
    int hw_id = ctx->hw_id;
    struct wcgModule *module = &layerToWcgMod[layer_index];
    auto *layer = &ctx->Layers[hw_id][layer_index];
    auto *Target = &ctx->Target;
    int input_dataspace = layer->dataspace & (HAL_DATASPACE_STANDARD_MASK | HAL_DATASPACE_TRANSFER_MASK);
    int output_dataspace = Target->dataspace & (HAL_DATASPACE_STANDARD_MASK | HAL_DATASPACE_TRANSFER_MASK);
    if (module->customTable.count(input_dataspace) &&
        module->customTable[input_dataspace].capa == Target->hdr_capa) {
        output_dataspace = module->customTable[input_dataspace].dataspace;
    }

    int input_transfer = (input_dataspace & HAL_DATASPACE_TRANSFER_MASK) >> HAL_DATASPACE_TRANSFER_SHIFT;
    int input_gamut = (input_dataspace & HAL_DATASPACE_STANDARD_MASK) >> HAL_DATASPACE_STANDARD_SHIFT;
    int output_transfer = (output_dataspace & HAL_DATASPACE_TRANSFER_MASK) >> HAL_DATASPACE_TRANSFER_SHIFT;
    int output_gamut = (output_dataspace & HAL_DATASPACE_STANDARD_MASK) >> HAL_DATASPACE_STANDARD_SHIFT;

    if (input_transfer == output_transfer && input_gamut == output_gamut) {
        layer->setInActive();
        for (auto iter = module->modEnTable[0].data.begin();
                iter != module->modEnTable[0].data.end(); iter++) {
            struct hdr_dat_node* dat = &iter->second.modEnCoef_packed;
            dat->queue_and_group(
                    layer->shall,
                    layer->group[hw_id]);
        }
    } else {
        layer->setActive();
        for (auto iter = module->modEnTable[1].data.begin();
                iter != module->modEnTable[1].data.end(); iter++) {
            struct hdr_dat_node* dat = &iter->second.modEnCoef_packed;
            dat->queue_and_group(
                    layer->shall,
                    layer->group[hw_id]);
        }

        for (auto iter = module->eotfTable[input_transfer].data.begin();
                iter != module->eotfTable[input_transfer].data.end(); iter++) {
            struct hdr_dat_node* dat = &iter->second.eotfCoef_packed;
            dat->queue_and_group(
                    layer->shall,
                    layer->group[hw_id]);
        }

        for (auto iter = module->oetfTable[output_transfer].data.begin();
                iter != module->oetfTable[output_transfer].data.end(); iter++) {
            struct hdr_dat_node* dat = &iter->second.oetfCoef_packed;
            dat->queue_and_group(
                    layer->shall,
                    layer->group[hw_id]);
        }

        for (auto iter = module->gmTable[input_gamut].out[output_gamut].data.begin();
                iter != module->gmTable[input_gamut].out[output_gamut].data.end(); iter++) {
            struct hdr_dat_node* dat = &iter->second.gmCoef_packed;
            dat->queue_and_group(
                    layer->shall,
                    layer->group[hw_id]);
        }
    }

    for (auto iter = layer->group[hw_id].begin(); iter != layer->group[hw_id].end(); iter++)
        layer->shall.push_back(&iter->second);

    return ret;
}

