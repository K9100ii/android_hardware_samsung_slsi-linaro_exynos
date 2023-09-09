#include "hdrContext.h"
#include <unistd.h>
#include "hdr10Coef.h"
#include "hdrCurveData.h"

using namespace std;

hdr10Coef::hdr10Coef(void) {
    hdr10info.infofile = "/vendor/etc/dqe/hdr10Info";
    hdr10info.hdr_nodes.clear();

    capStrMap = mapStringToCapa();
    stStrMap = mapStringToStandard();
    tfStrMap = mapStringToTransfer();
}

void hdr10Coef::parse_hdr10Node(
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

void hdr10Coef::parse_hdr10Mod(
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

void hdr10Coef::parse_hdr10Mods(
        int hw_id,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module)
{
    xmlNodePtr module = xml_module;
    xmlChar *module_id;
    std::vector<int> hdr10ModIds;
    while (module != NULL) {
        if (xmlStrcmp(module->name, (const xmlChar *)"hdr-module")) {
            module = module->next;
            continue;
        }
        module_id = xmlGetProp(module, (const xmlChar *)"layer");
        split(string((char*)module_id), ',', hdr10ModIds);
        xmlFree(module_id);

        for (auto &modId : hdr10ModIds) {
            struct hdr10Module hdr10Mod;
            parse_hdr10Mod(hw_id, modId, hdr10Mod, xml_doc, module->xmlChildrenNode);
            hdr10Mod.sort_ascending();
            layerToHdr10Mod.insert(make_pair(modId, hdr10Mod));
        }

        module = module->next;
    }
}

void hdr10Coef::__parse__(int hw_id, struct hdrContext *ctx)
{
    xmlDocPtr doc;
    xmlNodePtr root;

    std::string fn_target = hdr10info.getFileName(&ctx->Target);

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

void hdr10Coef::parse(std::vector<struct supportedHdrHw> *list, struct hdrContext *ctx)
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

void hdr10Coef::parse(hdrHwInfo *hwInfo, struct hdrContext *ctx)
{
    this->hwInfo = hwInfo;
    parse(hwInfo->getListHdrHw(), ctx);
    //this->dump();
}

void hdr10Coef::tm_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct hdrContext *ctx,
        struct tonemapModuleSpecifier *tmModSpecifier)
{
    if (tmModSpecifier->has) {
        if (!tmModSpecifier->isValidNit(layer->source_luminance))
            return;
        IHdrHw *IHw = hwInfo->getIf(hw_id);
        struct hdr_dat_node* dat;
        tmModSpecifier->mod_en[0] = 1;
        genTMCurve({layer->dataspace, layer->source_luminance, layer->target_luminance, ctx->metaIf},
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

void hdr10Coef::pq_coefBuildup(int hw_id, int layer_index,
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

void hdr10Coef::eotf_coefBuildup(int hw_id, int layer_index,
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

unsigned int hdr10Coef::getSourceLuminance(struct _HdrLayerInfo_ *layer)
{
    struct hdrContext *ctx = layer->ctx;
    unsigned int source_luminance = 1500;
    bool valid_mastering_luminance = layer->mastering_luminance > 100;
    bool valid_max_cll = layer->max_cll > 0;

    if (valid_mastering_luminance && valid_max_cll)
        source_luminance = min(layer->mastering_luminance, layer->max_cll);
    else if (valid_mastering_luminance)
        source_luminance = layer->mastering_luminance;
    else if (valid_max_cll)
        source_luminance = layer->max_cll;

    source_luminance = max(source_luminance, layer->target_luminance);

    return (ctx->source_luminance > 0) ? ctx->source_luminance : source_luminance;
}

int hdr10Coef::coefBuildup(int layer_index, struct hdrContext *ctx)
{
    int ret = HDR_ERR_NO;
    int hw_id = ctx->hw_id;
    struct hdr10Module *module = &layerToHdr10Mod[layer_index];
    auto layer = &ctx->Layers[hw_id][layer_index];
    bool found = false;

    layer->source_luminance = getSourceLuminance(layer);

    LIBHDR_LOGD(layer->log_level, "%s::%s + (lum %d->%d)", TAG, __func__,
                            layer->source_luminance, layer->target_luminance);

    struct tonemapModuleSpecifier *tmModSpecifier = ctx->moduleSpecifiers.getTonemapModuleSpecifier(layer_index);
    if (tmModSpecifier != NULL)
        tm_coefBuildup(hw_id, layer_index, layer, ctx, tmModSpecifier);
    struct pqModuleSpecifier *pqModSpecifier = ctx->moduleSpecifiers.getPqModuleSpecifier(layer_index);
    if (pqModSpecifier != NULL)
        pq_coefBuildup(hw_id, layer_index, layer, pqModSpecifier);
    struct eotfModuleSpecifier *eotfModSpecifier = ctx->moduleSpecifiers.getEotfModuleSpecifier(layer_index);
    if (eotfModSpecifier != NULL)
        eotf_coefBuildup(hw_id, layer_index, layer, eotfModSpecifier, pqModSpecifier);

    for (auto iter = module->hdr10NodeTable.begin();
            iter != module->hdr10NodeTable.end(); iter++) {
        if (iter->max_luminance == -1 ||
            layer->source_luminance == iter->max_luminance) {
            for (auto __iter__ = iter->coef_packed.begin();
                    __iter__ != iter->coef_packed.end(); __iter__++) {
                struct hdr_dat_node* dat = &(*__iter__);
                dat->queue_and_group(
                        layer->shall,
                        layer->group[hw_id]);
            }

            found = true;
        }
        if ((int)layer->source_luminance <= iter->max_luminance)
            break;
    }

    for (auto iter = layer->group[hw_id].begin(); iter != layer->group[hw_id].end(); iter++)
        layer->shall.push_back(&iter->second);

    if (found == false) {
        LIBHDR_LOGD(ctx->log_level, "hdr10Node not found on hdr10NodeTable(max lum = %d)", layer->source_luminance);
    }

    LIBHDR_LOGD(layer->log_level, "%s::%s -", TAG, __func__);

    return ret;
}

void hdr10Coef::init(struct hdrContext *ctx) {
    std::string fn_default = hdr10info.infofile + (std::string)".xml";
    std::string fn_target =  hdr10info.infofile + ctx->target_name;

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

    if (xmlStrcmp(root->name, (const xmlChar *)"HDR10_INFO")) {
        ALOGE("document of the wrong type, root node != HDR10_INFO");
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

void hdr10Coef::parse_hdrSubNodes(
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
    hdr10info.hdr_nodes.push_back(node);
}
void hdr10Coef::parse_hdrNodes(
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
