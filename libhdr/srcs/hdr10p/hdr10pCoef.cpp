#include "hdrContext.h"
#include <unistd.h>
#include "hdr10pCoef.h"
#include "hdrCurveData.h"

using namespace std;

hdr10pCoef::hdr10pCoef(void) {
    hdr10pinfo.infofile = "/vendor/etc/dqe/hdr10pInfo";
    hdr10pinfo.hdr_nodes.clear();

    capStrMap = mapStringToCapa();
    stStrMap = mapStringToStandard();
    tfStrMap = mapStringToTransfer();
}

void hdr10pCoef::parse_hdr10pNode(
        int hw_id,
        int module_id,
        struct hdr10pNode __attribute__((unused)) &hdr10p_node,
        xmlDocPtr __attribute__((unused)) xml_doc,
        xmlNodePtr xml_hdr10pnode)
{
    IHdrHw *IHw = hwInfo->getIf(hw_id);
    xmlNodePtr hdr10pnode = xml_hdr10pnode;
    while (hdr10pnode != NULL) {
        string subModName = string((char*)hdr10pnode->name);
        if (IHw->hasSubMod(module_id, subModName)) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                hdr10pnode->xmlChildrenNode, 1);
            std::vector<int> out_vec;
            split(string((char*)key), ',', out_vec);
            xmlFree(key);

            struct hdr_dat_node tmp;
            IHw->pack(subModName, module_id, out_vec, tmp);
            hdr10p_node.coef_packed.push_back(tmp);
        }
        hdr10pnode = hdr10pnode->next;
    }
}

void hdr10pCoef::parse_hdr10pMod(
        int hw_id,
        int module_id,
        struct hdr10pModule &hdr10p_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module)
{
    xmlNodePtr subInfo = xml_module;
    while (subInfo != NULL) {
        if (!xmlStrcmp(subInfo->name, (const xmlChar *)"hdr10p-node")) {
            struct hdr10pNode tmpNode;
            xmlChar *max_lum = xmlGetProp(subInfo, (const xmlChar *)"max-luminance");
            tmpNode.max_luminance = stoi(string((char*)max_lum));
            xmlFree(max_lum);

            parse_hdr10pNode(hw_id, module_id, tmpNode, xml_doc, subInfo->xmlChildrenNode);

            hdr10p_module.hdr10pNodeTable.push_back(tmpNode);
        }

        subInfo = subInfo->next;
    }
}

void hdr10pCoef::parse_hdr10pMods(
        int hw_id,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module)
{
    xmlNodePtr module = xml_module;
    xmlChar *module_id;
    std::vector<int> hdr10pModIds;
    while (module != NULL) {
        if (!xmlStrcmp(module->name, (const xmlChar *)"hdr-module")) {
            module_id = xmlGetProp(module, (const xmlChar *)"layer");
            split(string((char*)module_id), ',', hdr10pModIds);
            xmlFree(module_id);

            for (auto &modId : hdr10pModIds) {
                struct hdr10pModule hdr10pMod;
                parse_hdr10pMod(hw_id, modId, hdr10pMod, xml_doc, module->xmlChildrenNode);
                hdr10pMod.sort_ascending();
                layerToHdr10pMod.insert(make_pair(modId, hdr10pMod));
            }
        }
        module = module->next;
    }
}

void hdr10pCoef::__parse__(int hw_id, struct hdrContext *ctx)
{
    xmlDocPtr doc;
    xmlNodePtr root;

    std::string fn_target = hdr10pinfo.getFileName(&ctx->Target);

    doc = xmlParseFile(fn_target.c_str());
    if (doc == NULL) {
        ALOGD("no document or can not parse the document(%s)",
                fn_target.c_str());
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

    parse_hdr10pMods(hw_id, doc, root->xmlChildrenNode);

free_root:
    //xmlFree(root);
free_doc:
    xmlFreeDoc(doc);
ret:
    return;
}

void hdr10pCoef::parse(std::vector<struct supportedHdrHw> *list, struct hdrContext *ctx)
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

void hdr10pCoef::parse(hdrHwInfo *hwInfo, struct hdrContext *ctx)
{
    this->hwInfo = hwInfo;
    parse(hwInfo->getListHdrHw(), ctx);
    //this->dump();
}

void hdr10pCoef::tm_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct tonemapModuleSpecifier *tmModSpecifier)
{
    if (tmModSpecifier->has) {
        IHdrHw *IHw = hwInfo->getIf(hw_id);
        struct hdr_dat_node* dat;
        tmModSpecifier->mod_en[0] = 1;
        genTMCurve({layer->dataspace, layer->source_luminance, layer->target_luminance},
                &layer->dyn_meta,
                tmModSpecifier->mod_x, tmModSpecifier->mod_y, tmModSpecifier->size_x,
                tmModSpecifier->mod_x_bit, tmModSpecifier->mod_y_bit, tmModSpecifier->mod_minx_bit);

        IHw->pack(tmModSpecifier->mod_en_id, layer_index, tmModSpecifier->mod_en, tmModSpecifier->mod_en_packed);
        dat = &tmModSpecifier->mod_en_packed;
        dat->queue_and_group(
                layer->shall,
                layer->group[hw_id]);

        IHw->pack(tmModSpecifier->mod_x_id, layer_index, tmModSpecifier->mod_x, tmModSpecifier->mod_x_packed);
        dat = &tmModSpecifier->mod_x_packed;
        dat->queue_and_group(
                layer->shall,
                layer->group[hw_id]);

        IHw->pack(tmModSpecifier->mod_y_id, layer_index, tmModSpecifier->mod_y, tmModSpecifier->mod_y_packed);
        dat = &tmModSpecifier->mod_y_packed;
        dat->queue_and_group(
                layer->shall,
                layer->group[hw_id]);
    }
}

void hdr10pCoef::pq_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct pqModuleSpecifier *pqModSpecifier)
{
    if (pqModSpecifier->has) {
        if (!pqModSpecifier->isValidNit(layer->source_luminance))
            return;
        pqModSpecifier->genEotfPQ(layer->source_luminance);

        IHdrHw *IHw = hwInfo->getIf(hw_id);
        struct hdr_dat_node* dat;

        IHw->pack(pqModSpecifier->mod_en_id, layer_index, pqModSpecifier->mod_en, pqModSpecifier->mod_en_packed);
        dat = &pqModSpecifier->mod_en_packed;
        dat->queue_and_group(
                layer->shall,
                layer->group[hw_id]);

        IHw->pack(pqModSpecifier->pq_en_id, layer_index, pqModSpecifier->pq_en, pqModSpecifier->pq_en_packed);
        dat = &pqModSpecifier->pq_en_packed;
        dat->queue_and_group(
                layer->shall,
                layer->group[hw_id]);

        IHw->pack(pqModSpecifier->coef_id, layer_index, pqModSpecifier->coef, pqModSpecifier->coef_packed);
        dat = &pqModSpecifier->coef_packed;
        dat->queue_and_group(
                layer->shall,
                layer->group[hw_id]);

        IHw->pack(pqModSpecifier->shift_id, layer_index, pqModSpecifier->shift, pqModSpecifier->shift_packed);
        dat = &pqModSpecifier->shift_packed;
        dat->queue_and_group(
                layer->shall,
                layer->group[hw_id]);
    }
}

void hdr10pCoef::eotf_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct eotfModuleSpecifier *eotfModSpecifier,
        struct pqModuleSpecifier *pqModSpecifier)
{
    if (pqModSpecifier && pqModSpecifier->has &&
        pqModSpecifier->isValidNit(layer->source_luminance))
        return;

    if (eotfModSpecifier->has) {
        if (!eotfModSpecifier->isValidNit(layer->source_luminance))
            return;
        IHdrHw *IHw = hwInfo->getIf(hw_id);
        struct hdr_dat_node* dat;
        eotfModSpecifier->mod_en[0] = 1;
        genEOTFCurve({layer->dataspace, layer->source_luminance, 0},
                eotfModSpecifier->mod_x, eotfModSpecifier->mod_y, eotfModSpecifier->size_x,
                eotfModSpecifier->mod_x_bit, eotfModSpecifier->mod_y_bit, eotfModSpecifier->mod_minx_bit);

        IHw->pack(eotfModSpecifier->mod_en_id, layer_index, eotfModSpecifier->mod_en, eotfModSpecifier->mod_en_packed);
        dat = &eotfModSpecifier->mod_en_packed;
        dat->queue_and_group(
                layer->shall,
                layer->group[hw_id]);

        IHw->pack(eotfModSpecifier->mod_x_id, layer_index, eotfModSpecifier->mod_x, eotfModSpecifier->mod_x_packed);
        dat = &eotfModSpecifier->mod_x_packed;
        dat->queue_and_group(
                layer->shall,
                layer->group[hw_id]);

        IHw->pack(eotfModSpecifier->mod_y_id, layer_index, eotfModSpecifier->mod_y, eotfModSpecifier->mod_y_packed);
        dat = &eotfModSpecifier->mod_y_packed;
        dat->queue_and_group(
                layer->shall,
                layer->group[hw_id]);
    }
}

int hdr10pCoef::coefBuildup(int layer_index, struct hdrContext *ctx)
{
    int ret = HDR_ERR_NO;
    int hw_id = ctx->hw_id;
    struct hdr10pModule *module = &layerToHdr10pMod[layer_index];
    auto layer = &ctx->Layers[hw_id][layer_index];
    bool found = false;

    layer->source_luminance = getMaxLuminance(layer->target_luminance, layer->dyn_meta);

    LIBHDR_LOGD(layer->log_level, "%s::%s + (lum %d->%d)", TAG, __func__,
                            layer->source_luminance, layer->target_luminance);

    if (layer->validateDynamicMeta() == HDR_ERR_INVAL)
        return HDR_ERR_INVAL;

    struct tonemapModuleSpecifier *tmModSpecifier = ctx->moduleSpecifiers.getTonemapModuleSpecifier(layer_index);
    if (tmModSpecifier != NULL)
        tm_coefBuildup(hw_id, layer_index, layer, tmModSpecifier);
    struct pqModuleSpecifier *pqModSpecifier = ctx->moduleSpecifiers.getPqModuleSpecifier(layer_index);
    if (pqModSpecifier != NULL)
        pq_coefBuildup(hw_id, layer_index, layer, pqModSpecifier);
    struct eotfModuleSpecifier *eotfModSpecifier = ctx->moduleSpecifiers.getEotfModuleSpecifier(layer_index);
    if (eotfModSpecifier != NULL)
        eotf_coefBuildup(hw_id, layer_index, layer, eotfModSpecifier, pqModSpecifier);

    for (auto iter = module->hdr10pNodeTable.begin();
            iter != module->hdr10pNodeTable.end(); iter++) {
        if (layer->source_luminance <= iter->max_luminance) {
            for (auto __iter__ = iter->coef_packed.begin();
                    __iter__ != iter->coef_packed.end(); __iter__++) {
                struct hdr_dat_node* dat = &(*__iter__);
                dat->queue_and_group(
                        layer->shall,
                        layer->group[hw_id]);
            }

            found = true;
            break;
        }
    }

    for (auto iter = layer->group[hw_id].begin(); iter != layer->group[hw_id].end(); iter++)
        layer->shall.push_back(&iter->second);

    if (found == false) {
        LIBHDR_LOGD(ctx->log_level, "hdr10pNode not found on hdr10pNodeTable(max lum = %d)", layer->source_luminance);
    }

    LIBHDR_LOGD(layer->log_level, "%s::%s -", TAG, __func__);

    return ret;
}

void hdr10pCoef::init(struct hdrContext *ctx) {
    std::string fn_default = hdr10pinfo.infofile + (std::string)".xml";
    std::string fn_target =  hdr10pinfo.infofile + ctx->target_name;

    xmlDocPtr doc;
    xmlNodePtr root;

    doc = xmlParseFile(fn_target.c_str());
    if (doc == NULL) {
        if (doc == NULL)
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

    if (xmlStrcmp(root->name, (const xmlChar *)"HDR10P_INFO")) {
        ALOGE("document of the wrong type, root node != HDR10P_INFO");
        goto free_root;
    }

    parse_hdrNodes(doc, root->xmlChildrenNode);

free_root:
    //xmlFree(root);
free_doc:
    xmlFreeDoc(doc);
ret:
    return;
}

void hdr10pCoef::parse_hdrSubNodes(
            xmlDocPtr xml_doc,
            xmlNodePtr xml_subnodes)
{
    xmlNodePtr subnode = xml_subnodes;
    struct hdr_node node = {HDR_CAPA_MAX,0,0,""};
    while (subnode != NULL) {
        xmlChar *key = xmlNodeListGetString(xml_doc,
            subnode->xmlChildrenNode, 1);
        if (!xmlStrcmp(subnode->name, (const xmlChar *)"capa")) {
            node.capa = capStrMap[(char*)key];
        } else if (!xmlStrcmp(subnode->name, (const xmlChar *)"gamut")) {
            node.gamut = stStrMap[(char*)key];
        } else if (!xmlStrcmp(subnode->name, (const xmlChar *)"transfer-function")) {
            node.transfer_function = tfStrMap[(char*)key];
        } else if (!xmlStrcmp(subnode->name, (const xmlChar *)"filename")) {
            node.filename = (std::string)(char*)key;
        }
        xmlFree(key);
        subnode = subnode->next;
    }
    hdr10pinfo.hdr_nodes.push_back(node);
}
void hdr10pCoef::parse_hdrNodes(
            xmlDocPtr xml_doc,
            xmlNodePtr xml_nodes)
{
    xmlNodePtr node = xml_nodes;
    while (node != NULL) {
        if (xmlStrcmp(node->name, (const xmlChar *)"hdr-node"))
            goto next;
        parse_hdrSubNodes(xml_doc, node->xmlChildrenNode);
next:
        node = node->next;
    }
}
