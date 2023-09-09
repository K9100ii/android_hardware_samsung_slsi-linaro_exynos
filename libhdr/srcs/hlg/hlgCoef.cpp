#include "hdrContext.h"
#include <unistd.h>
#include "hlgCoef.h"
#include "hdrCurveData.h"

using namespace std;

hlgCoef::hlgCoef(void) {
    hlginfo.infofile = "/vendor/etc/dqe/hlgInfo";
    hlginfo.hdr_nodes.clear();

    capStrMap = mapStringToCapa();
    stStrMap = mapStringToStandard();
    tfStrMap = mapStringToTransfer();
}

void hlgCoef::parse_hlgNode(
        int hw_id,
        int module_id,
        struct hlgNode __attribute__((unused)) &hlg_node,
        xmlDocPtr __attribute__((unused)) xml_doc,
        xmlNodePtr xml_hlgnode)
{
    IHdrHw *IHw = hwInfo->getIf(hw_id);
    xmlNodePtr hlgnode = xml_hlgnode;
    while (hlgnode != NULL) {
        string subModName = string((char*)hlgnode->name);
        if (IHw->hasSubMod(module_id, subModName)) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                hlgnode->xmlChildrenNode, 1);
            std::vector<int> out_vec;
            split(string((char*)key), ',', out_vec);
            xmlFree(key);

            struct hdr_dat_node tmp;
            IHw->pack(subModName, module_id, out_vec, tmp);

            hlg_node.coef_packed.push_back(tmp);
        }
        hlgnode = hlgnode->next;
    }
}

void hlgCoef::parse_hlgMod(
        int hw_id,
        int module_id,
        struct hlgModule &hlg_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module)
{
    xmlNodePtr subInfo = xml_module;
    while (subInfo != NULL) {
        if (!xmlStrcmp(subInfo->name, (const xmlChar *)"hlg-node")) {
            struct hlgNode tmpNode;

            parse_hlgNode(hw_id, module_id, tmpNode, xml_doc, subInfo->xmlChildrenNode);

            hlg_module.hlgNodeTable.push_back(tmpNode);
        }
        subInfo = subInfo->next;
    }
}

void hlgCoef::parse_hlgMods(
        int hw_id,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module)
{
    xmlNodePtr module = xml_module;
    xmlChar *module_id;
    std::vector<int> hlgModIds;
    while (module != NULL) {
        if (xmlStrcmp(module->name, (const xmlChar *)"hdr-module")) {
            module = module->next;
            continue;
        }
        module_id = xmlGetProp(module, (const xmlChar *)"layer");
        split(string((char*)module_id), ',', hlgModIds);
        xmlFree(module_id);

        for (auto &modId : hlgModIds) {
            struct hlgModule hlgMod;
            parse_hlgMod(hw_id, modId, hlgMod, xml_doc, module->xmlChildrenNode);
            layerToHlgMod.insert(make_pair(modId, hlgMod));
        }

        module = module->next;
    }
}

void hlgCoef::__parse__(int hw_id, struct hdrContext *ctx)
{
    xmlDocPtr doc;
    xmlNodePtr root;

    std::string fn_target = hlginfo.getFileName(&ctx->Target);

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

    if (xmlStrcmp(root->name, (const xmlChar *)"HLG_LUT")) {
        ALOGE("document of the wrong type, root node != HLG_LUT");
        goto free_root;
    }

    parse_hlgMods(hw_id, doc, root->xmlChildrenNode);

free_root:
    //xmlFree(root);
free_doc:
    xmlFreeDoc(doc);
ret:
    return;
}

void hlgCoef::parse(std::vector<struct supportedHdrHw> *list, struct hdrContext *ctx)
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

void hlgCoef::parse(hdrHwInfo *hwInfo, struct hdrContext *ctx)
{
    this->hwInfo = hwInfo;
    parse(hwInfo->getListHdrHw(), ctx);
    //this->dump();
}

void hlgCoef::tm_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct tonemapModuleSpecifier *tmModSpecifier)
{
    if (tmModSpecifier->has) {
        IHdrHw *IHw = hwInfo->getIf(hw_id);
        struct hdr_dat_node* dat;
        tmModSpecifier->mod_en[0] = 1;
        genTMCurve({layer->dataspace, layer->source_luminance, layer->target_luminance},
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

void hlgCoef::eotf_coefBuildup(int hw_id, int layer_index,
        struct _HdrLayerInfo_ *layer,
        struct eotfModuleSpecifier *eotfModSpecifier)
{
    if (eotfModSpecifier->has) {
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

unsigned int hlgCoef::getSourceLuminance()
{
    return 1000; // always sets 1000 nit regardless of static meta
}

int hlgCoef::coefBuildup(int layer_index, struct hdrContext *ctx)
{
    int ret = HDR_ERR_NO;
    int hw_id = ctx->hw_id;
    struct hlgModule *module = NULL;
    auto layer = &ctx->Layers[hw_id][layer_index];

    layer->source_luminance = getSourceLuminance();

    LIBHDR_LOGD(layer->log_level, "%s::%s + (lum %d->%d)", TAG, __func__,
                            layer->source_luminance, layer->target_luminance);

    if (layerToHlgMod.count(layer_index) == 0) {
        ALOGD("hlg module node not found");
        return HDR_ERR_INVAL;
    }

    module = &layerToHlgMod[layer_index];
    if (module->hlgNodeTable.size() == 0) {
        ALOGD("hlg node not found");
        return HDR_ERR_INVAL;
    }

    struct tonemapModuleSpecifier *tmModSpecifier = ctx->moduleSpecifiers.getTonemapModuleSpecifier(layer_index);
    if (tmModSpecifier != NULL)
        tm_coefBuildup(hw_id, layer_index, layer, tmModSpecifier);
    struct eotfModuleSpecifier *eotfModSpecifier = ctx->moduleSpecifiers.getEotfModuleSpecifier(layer_index);
    if (eotfModSpecifier != NULL)
        eotf_coefBuildup(hw_id, layer_index, layer, eotfModSpecifier);

    for (auto iter = module->hlgNodeTable[0].coef_packed.begin();
            iter != module->hlgNodeTable[0].coef_packed.end(); iter++) {
        struct hdr_dat_node* dat = &(*iter);
        dat->queue_and_group(
                layer->shall,
                layer->group[hw_id]);
    }

    for (auto iter = layer->group[hw_id].begin(); iter != layer->group[hw_id].end(); iter++)
        layer->shall.push_back(&iter->second);

    LIBHDR_LOGD(layer->log_level, "%s::%s -", TAG, __func__);

    return ret;
}

void hlgCoef::init(struct hdrContext *ctx) {
    std::string fn_default = hlginfo.infofile + (std::string)".xml";
    std::string fn_target =  hlginfo.infofile + ctx->target_name;

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

    if (xmlStrcmp(root->name, (const xmlChar *)"HLG_INFO")) {
        ALOGE("document of the wrong type, root node != HLG_INFO");
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

void hlgCoef::parse_hdrSubNodes(
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
    hlginfo.hdr_nodes.push_back(node);
}
void hlgCoef::parse_hdrNodes(
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
