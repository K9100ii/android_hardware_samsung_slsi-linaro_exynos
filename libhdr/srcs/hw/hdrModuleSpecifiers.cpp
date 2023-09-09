#include <hdrModuleSpecifiers.h>
#include <hdrUtil.h>

void hdrModuleSpecifiers::init(hdrHwInfo *hwInfo)
{
    this->hwInfo = hwInfo;

    xmlDocPtr doc;
    xmlNodePtr root;

    doc = xmlParseFile(filename.c_str());
    if (doc == NULL) {
        ALOGD("no document or can not parse the default document(%s)",
                filename.c_str());
        goto ret;
    }
    root = xmlDocGetRootElement(doc);
    if (root == NULL) {
        ALOGE("empty document");
        goto free_doc;
    }
    if (xmlStrcmp(root->name, (const xmlChar *)"HDR_MODULE_SPECIFIERS")) {
        ALOGE("document of the wrong type, root node != HDR_MODULE_SPECIFIERS");
        goto free_root;
    }
    parse_hdrModuleSpecifiers(doc, root->xmlChildrenNode);
free_root:
    //xmlFree(root);
free_doc:
    xmlFreeDoc(doc);
ret:
    return;
}

void hdrModuleSpecifiers::parse_hdrModuleSpecifiers(
            xmlDocPtr xml_doc,
            xmlNodePtr xml_modInfo)
{
    xmlNodePtr node = xml_modInfo;
    xmlChar *module_id;
    std::vector<int> hdrModIds;
    while (node != NULL) {
        struct hdrModuleSpecifier hdrModSpecifier;
        if (xmlStrcmp(node->name, (const xmlChar *)"hdr-module-specifier"))
            goto next;
        module_id = xmlGetProp(node, (const xmlChar *)"layer");
        split(std::string((char*)module_id), ',', hdrModIds);
        xmlFree(module_id);

        for (auto &modId : hdrModIds) {
            parse_hdrModuleSpecifier(modId, xml_doc, node->xmlChildrenNode, hdrModSpecifier);
            layerToHdrModSpecifier[modId] = hdrModSpecifier;
        }
next:
        node = node->next;
    }
}

void hdrModuleSpecifiers::parse_hdrModuleSpecifier(
            int module_id,
            xmlDocPtr xml_doc,
            xmlNodePtr xml_mod,
            struct hdrModuleSpecifier &hdrModSpecifier)
{
    xmlNodePtr node = xml_mod;
    while (node != NULL) {
        if (!xmlStrcmp(node->name, (const xmlChar *)"tonemap-module-specifier"))
            parse_tonemapModSpecifier(module_id, xml_doc, node->xmlChildrenNode, hdrModSpecifier.tmModSpecifier);
        else if (!xmlStrcmp(node->name, (const xmlChar *)"pq-module-specifier"))
            parse_pqModSpecifier(module_id, xml_doc, node->xmlChildrenNode, hdrModSpecifier.pqModSpecifier);
        else if (!xmlStrcmp(node->name, (const xmlChar *)"eotf-module-specifier"))
            parse_eotfModSpecifier(module_id, xml_doc, node->xmlChildrenNode, hdrModSpecifier.eotfModSpecifier);
        node = node->next;
    }
}

void hdrModuleSpecifiers::parse_tonemapModSpecifier(
            int module_id,
            xmlDocPtr xml_doc,
            xmlNodePtr xml_tm,
            struct tonemapModuleSpecifier &tmMod) {
    int hw_id = (int)HDR_HW_DPU;
    xmlNodePtr node = xml_tm;
    IHdrHw *IHw = hwInfo->getIf(hw_id);
    bool checked = false;
    tmMod.init();
    while (node != NULL) {
        if (!xmlStrcmp(node->name, (const xmlChar *)"mod-en")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                node->xmlChildrenNode, 1);
            tmMod.mod_en_id = std::string((char*)key);
            checked = true;
            xmlFree(key);
        } else if (!xmlStrcmp(node->name, (const xmlChar *)"mod-x")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                node->xmlChildrenNode, 1);
            tmMod.mod_x_id = std::string((char*)key);
            checked = true;
            xmlFree(key);
        } else if (!xmlStrcmp(node->name, (const xmlChar *)"mod-y")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                node->xmlChildrenNode, 1);
            tmMod.mod_y_id = std::string((char*)key);
            checked = true;
            xmlFree(key);
        } else if (!xmlStrcmp(node->name, (const xmlChar *)"mod-x-bit")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                node->xmlChildrenNode, 1);
            tmMod.mod_x_bit = stoi(std::string((char*)key));
            checked = true;
            xmlFree(key);
        } else if (!xmlStrcmp(node->name, (const xmlChar *)"mod-y-bit")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                node->xmlChildrenNode, 1);
            tmMod.mod_y_bit = stoi(std::string((char*)key));
            checked = true;
            xmlFree(key);
        } else if (!xmlStrcmp(node->name, (const xmlChar *)"mod-minx-bit")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                node->xmlChildrenNode, 1);
            tmMod.mod_minx_bit = stoi(std::string((char*)key));
            checked = true;
            xmlFree(key);
        }
        node = node->next;
    }
    if (checked &&
            IHw->hasSubMod(module_id, tmMod.mod_x_id) &&
            IHw->hasSubMod(module_id, tmMod.mod_y_id) &&
            IHw->hasSubMod(module_id, tmMod.mod_en_id)) {
        tmMod.has = true;
        tmMod.mod_en.resize(IHw->getSubModNodeNum(tmMod.mod_en_id, module_id));
        tmMod.mod_x.resize(IHw->getSubModNodeNum(tmMod.mod_x_id, module_id));
        tmMod.size_x = IHw->getSubModNodeNum(tmMod.mod_x_id, module_id);
        tmMod.mod_y.resize(IHw->getSubModNodeNum(tmMod.mod_y_id, module_id));
        tmMod.size_y = IHw->getSubModNodeNum(tmMod.mod_y_id, module_id);
        //ALOGD("%s: tone_map x(%d), y(%d)",
        //    __func__,
        //    IHw->getSubModNodeNum(tmMod.mod_x_id, module_id),
        //    IHw->getSubModNodeNum(tmMod.mod_y_id, module_id)); EOL(level);
    } else if (checked){
        tmMod.has = false;
        ALOGD("%s: no tonemap sub hw named (%s, %s, %s)",
            __func__, tmMod.mod_en_id.c_str(),
            tmMod.mod_x_id.c_str(), tmMod.mod_y_id.c_str());
    } else {
        tmMod.has = false;
        ALOGD("%s: no tonemap specified", __func__);
    }
}

void hdrModuleSpecifiers::parse_pqModSpecifier(
        int module_id,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_pq,
        struct pqModuleSpecifier &pqMod)
{
    int hw_id = (int)HDR_HW_DPU;
    IHdrHw *IHw = hwInfo->getIf(hw_id);
    xmlNodePtr pqModNode = xml_pq;
    bool checked = false;

    pqMod.init();
    while (pqModNode != NULL) {
        if (!xmlStrcmp(pqModNode->name, (const xmlChar *)"mod-en")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                pqModNode->xmlChildrenNode, 1);
            pqMod.mod_en_id = std::string((char*)key);
            checked = true;
            xmlFree(key);
        } else if (!xmlStrcmp(pqModNode->name, (const xmlChar *)"pq-en")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                pqModNode->xmlChildrenNode, 1);
            pqMod.pq_en_id = std::string((char*)key);
            checked = true;
            xmlFree(key);
        } else if (!xmlStrcmp(pqModNode->name, (const xmlChar *)"coef")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                pqModNode->xmlChildrenNode, 1);
            pqMod.coef_id = std::string((char*)key);
            checked = true;
            xmlFree(key);
        } else if (!xmlStrcmp(pqModNode->name, (const xmlChar *)"shift")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                pqModNode->xmlChildrenNode, 1);
            pqMod.shift_id = std::string((char*)key);
            checked = true;
            xmlFree(key);
        }
        pqModNode = pqModNode->next;
    }
    if (checked &&
            IHw->hasSubMod(module_id, pqMod.mod_en_id) &&
            IHw->hasSubMod(module_id, pqMod.pq_en_id) &&
            IHw->hasSubMod(module_id, pqMod.coef_id) &&
            IHw->hasSubMod(module_id, pqMod.shift_id)) {
        pqMod.has = true;
        pqMod.mod_en.resize(IHw->getSubModNodeNum(pqMod.mod_en_id, module_id));
        pqMod.pq_en.resize(IHw->getSubModNodeNum(pqMod.pq_en_id, module_id));
        pqMod.coef.resize(IHw->getSubModNodeNum(pqMod.coef_id, module_id));
        pqMod.shift.resize(IHw->getSubModNodeNum(pqMod.shift_id, module_id));
    } else if (checked){
        ALOGD("%s: no pq sub hw named (%s, %s, %s)",
            __func__, pqMod.pq_en_id.c_str(),
            pqMod.coef_id.c_str(), pqMod.shift_id.c_str());
    } else {
        pqMod.has = false;
        ALOGD("%s: no pq sub hw specified", __func__);
    }
}

void hdrModuleSpecifiers::parse_eotfModSpecifier(
            int module_id,
            xmlDocPtr xml_doc,
            xmlNodePtr xml_eotf,
            struct eotfModuleSpecifier &eotfMod) {
    int hw_id = (int)HDR_HW_DPU;
    xmlNodePtr node = xml_eotf;
    IHdrHw *IHw = hwInfo->getIf(hw_id);
    bool checked = false;
    eotfMod.init();
    while (node != NULL) {
        if (!xmlStrcmp(node->name, (const xmlChar *)"mod-en")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                node->xmlChildrenNode, 1);
            eotfMod.mod_en_id = std::string((char*)key);
            checked = true;
            xmlFree(key);
        } else if (!xmlStrcmp(node->name, (const xmlChar *)"mod-x")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                node->xmlChildrenNode, 1);
            eotfMod.mod_x_id = std::string((char*)key);
            checked = true;
            xmlFree(key);
        } else if (!xmlStrcmp(node->name, (const xmlChar *)"mod-y")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                node->xmlChildrenNode, 1);
            eotfMod.mod_y_id = std::string((char*)key);
            checked = true;
            xmlFree(key);
        } else if (!xmlStrcmp(node->name, (const xmlChar *)"mod-x-bit")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                node->xmlChildrenNode, 1);
            eotfMod.mod_x_bit = stoi(std::string((char*)key));
            checked = true;
            xmlFree(key);
        } else if (!xmlStrcmp(node->name, (const xmlChar *)"mod-y-bit")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                node->xmlChildrenNode, 1);
            eotfMod.mod_y_bit = stoi(std::string((char*)key));
            checked = true;
            xmlFree(key);
        } else if (!xmlStrcmp(node->name, (const xmlChar *)"mod-minx-bit")) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                node->xmlChildrenNode, 1);
            eotfMod.mod_minx_bit = stoi(std::string((char*)key));
            checked = true;
            xmlFree(key);
        }
        node = node->next;
    }
    if (checked &&
            IHw->hasSubMod(module_id, eotfMod.mod_x_id) &&
            IHw->hasSubMod(module_id, eotfMod.mod_y_id) &&
            IHw->hasSubMod(module_id, eotfMod.mod_en_id)) {
        eotfMod.has = true;
        eotfMod.mod_en.resize(IHw->getSubModNodeNum(eotfMod.mod_en_id, module_id));
        eotfMod.mod_x.resize(IHw->getSubModNodeNum(eotfMod.mod_x_id, module_id));
        eotfMod.size_x = IHw->getSubModNodeNum(eotfMod.mod_x_id, module_id);
        eotfMod.mod_y.resize(IHw->getSubModNodeNum(eotfMod.mod_y_id, module_id));
        eotfMod.size_y = IHw->getSubModNodeNum(eotfMod.mod_y_id, module_id);
        //ALOGD("%s: tone_map x(%d), y(%d)",
        //    __func__,
        //    IHw->getSubModNodeNum(eotfMod.mod_x_id, module_id),
        //    IHw->getSubModNodeNum(eotfMod.mod_y_id, module_id)); EOL(level);
    } else if (checked){
        eotfMod.has = false;
        ALOGD("%s: no eotf sub hw named (%s, %s, %s)",
            __func__, eotfMod.mod_en_id.c_str(),
            eotfMod.mod_x_id.c_str(), eotfMod.mod_y_id.c_str());
    } else {
        eotfMod.has = false;
        ALOGD("%s: no eotf specified", __func__);
    }
}

struct tonemapModuleSpecifier *hdrModuleSpecifiers::getTonemapModuleSpecifier(int layer_index)
{
    if (layerToHdrModSpecifier.count(layer_index) == 0)
        return NULL;
    return &layerToHdrModSpecifier[layer_index].tmModSpecifier;
}

struct pqModuleSpecifier *hdrModuleSpecifiers::getPqModuleSpecifier(int layer_index)
{
    if (layerToHdrModSpecifier.count(layer_index) == 0)
        return NULL;
    return &layerToHdrModSpecifier[layer_index].pqModSpecifier;
}

struct eotfModuleSpecifier *hdrModuleSpecifiers::getEotfModuleSpecifier(int layer_index)
{
    if (layerToHdrModSpecifier.count(layer_index) == 0)
        return NULL;
    return &layerToHdrModSpecifier[layer_index].eotfModSpecifier;
}
