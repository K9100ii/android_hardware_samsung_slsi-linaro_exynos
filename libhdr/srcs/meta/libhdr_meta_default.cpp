#include <hdrMetaInterface.h>

class hdrMetaImplementation: public hdrMetaInterface {
};

hdrMetaInterface *hdrMetaInterface::createInstance(void) {
    return nullptr;
}

