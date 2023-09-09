#include "hdrHwInfo.h"
#include "hdrUtil.h"

using namespace std;

void hdrHwInfo::init(void)
{
    for (int i = 0; i < listHdrHw.size(); i++) {
        idToIHdr[listHdrHw[i].id] = listHdrHw[i].IHw;
        idToStrId[listHdrHw[i].id] = listHdrHw[i].strId;
        strIdToId[listHdrHw[i].strId] = listHdrHw[i].id;
        idToFileName[listHdrHw[i].id] = listHdrHw[i].hdrHwInfoFile;

        listHdrHw[i].IHw->parse(hdrHwInfoid, listHdrHw[i].hdrHwInfoFile);
    }
}

IHdrHw *hdrHwInfo::getIf (int hw_id)
{
    return idToIHdr[hw_id];
}

std::vector<struct supportedHdrHw> *hdrHwInfo::getListHdrHw(void)
{
    return &listHdrHw;
}
