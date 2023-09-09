#include "hdrContext.h"
#include <unistd.h>
#include "hdrTuneCoef.h"

using namespace std;

void hdrTuneCoef::parse_hdrTuneNode(
        int hw_id,
        int module_id,
        struct hdrTuneNode __attribute__((unused)) &hdrTune_node,
        xmlDocPtr __attribute__((unused)) xml_doc,
        xmlNodePtr xml_hdrTunenode)
{
    IHdrHw *IHw = hwInfo->getIf(hw_id);
    xmlNodePtr hdrTunenode = xml_hdrTunenode;
    while (hdrTunenode != NULL) {
        string subModName = string((char*)hdrTunenode->name);
        if (IHw->hasSubMod(module_id, subModName)) {
            xmlChar *key = xmlNodeListGetString(xml_doc,
                                hdrTunenode->xmlChildrenNode, 1);
            std::vector<int> out_vec;
            split(string((char*)key), ',', out_vec);
            xmlFree(key);

            struct hdr_dat_node tmp;
            IHw->pack(subModName, module_id, out_vec, tmp);

            hdrTune_node.coef_packed.push_back(tmp);
        }
        hdrTunenode = hdrTunenode->next;
    }
}

void hdrTuneCoef::parse_hdrTuneMod(
        int hw_id,
        int module_id,
        struct hdrTuneModule &hdrTune_module,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module)
{
    xmlNodePtr subInfo = xml_module;
    while (subInfo != NULL) {
        if (!xmlStrcmp(subInfo->name, (const xmlChar *)"hdr-tune-node")) {
            struct hdrTuneNode tmpNode;

            parse_hdrTuneNode(hw_id, module_id, tmpNode, xml_doc, subInfo->xmlChildrenNode);

            hdrTune_module.hdrTuneNodeTable.push_back(tmpNode);
        }
        subInfo = subInfo->next;
    }
}

void hdrTuneCoef::parse_hdrTuneMods(
        int hw_id,
        xmlDocPtr xml_doc,
        xmlNodePtr xml_module)
{
    xmlNodePtr module = xml_module;
    xmlChar *module_id;
    std::vector<int> hdrTuneModIds;
    while (module != NULL) {
        if (xmlStrcmp(module->name, (const xmlChar *)"hdr-module")) {
            module = module->next;
            continue;
        }
        module_id = xmlGetProp(module, (const xmlChar *)"layer");
        split(string((char*)module_id), ',', hdrTuneModIds);
        xmlFree(module_id);

        for (auto &modId : hdrTuneModIds) {
            struct hdrTuneModule hdrTuneMod;
            parse_hdrTuneMod(hw_id, modId, hdrTuneMod, xml_doc, module->xmlChildrenNode);
            layerToHdrTuneMod[modId] = hdrTuneMod;
        }

        module = module->next;
    }
}

void hdrTuneCoef::__parse__(int hw_id, struct hdrContext *ctx)
{
    xmlDocPtr doc;
    xmlNodePtr root;

    std::string fn_default = tune_filename + (std::string)".xml";
    std::string fn_target = tune_filename + ctx->target_name;
    std::string fn_vendor = vendor_filename + (std::string)".xml";

    doc = xmlParseFile(fn_target.c_str());
    if (doc == NULL) {
        ALOGD("no document or can not parse the document(%s)",
                fn_target.c_str());
        doc = xmlParseFile(fn_default.c_str());
        if (doc == NULL) {
            ALOGD("no document or can not parse the default document(%s)",
                fn_default.c_str());
            doc = xmlParseFile(fn_vendor.c_str());
            if (doc == NULL) {
                ALOGD("no document or can not parse the default document(%s)",
                    fn_vendor.c_str());
                goto ret;
            }
        }
    }

    root = xmlDocGetRootElement(doc);
    if (root == NULL) {
        ALOGE("empty document");
        goto free_doc;
    }

    if (xmlStrcmp(root->name, (const xmlChar *)"HDR_TUNE_LUT")) {
        ALOGE("document of the wrong type, root node != HDR_TUNE_LUT");
        goto free_root;
    }

    parse_hdrTuneMods(hw_id, doc, root->xmlChildrenNode);

free_root:
    //xmlFree(root);
free_doc:
    xmlFreeDoc(doc);
ret:
    return;
}

void hdrTuneCoef::parse(std::vector<struct supportedHdrHw> *list, struct hdrContext *ctx)
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

void hdrTuneCoef::init(hdrHwInfo *hwInfo, struct hdrContext *ctx)
{
    this->hwInfo = hwInfo;
    parse(hwInfo->getListHdrHw(), ctx);
    //this->dump();
}

int hdrTuneCoef::coefBuildup(int layer_index, struct hdrContext *ctx)
{
    int ret = HDR_ERR_NO;
    int hw_id = ctx->hw_id;
    struct hdrTuneModule *module = NULL;
    auto layer = &ctx->Layers[hw_id][layer_index];

    LIBHDR_LOGD(layer->log_level, "%s::%s +", TAG, __func__);

    if (ctx->tune_reload == true) {
        ctx->tune_reload = false;
        this->parse(this->hwInfo->getListHdrHw(), ctx);
        ALOGD("tune coef reloading");
    }

    if (layerToHdrTuneMod.count(layer_index) == 0) {
        ALOGD("tune module node not found");
        return HDR_ERR_INVAL;
    }

    module = &layerToHdrTuneMod[layer_index];
    if (module->hdrTuneNodeTable.size() == 0) {
        ALOGD("tune node not found");
        return HDR_ERR_INVAL;
    }

    for (auto iter = module->hdrTuneNodeTable[0].coef_packed.begin();
            iter != module->hdrTuneNodeTable[0].coef_packed.end(); iter++) {
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

