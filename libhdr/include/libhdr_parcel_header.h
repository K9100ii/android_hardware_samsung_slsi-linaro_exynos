#ifndef __LIBHDR_PARCEL_HEADER__
#define __LIBHDR_PARCEL_HEADER__
#include<string.h>
#include<vector>
#include<list>
#include "hdrUtil.h"

#define PACK_ONE(packed, luts, nth, num_per_reg, align, masks, bit_offsets)     \
    do {                                                                        \
        packed = 0;                                                             \
        int j = 0;                                                              \
        switch (align) {                                                        \
        case 1:            \
            for (j = nth; j < (nth + num_per_reg); j++)                         \
                packed |= ((luts[j] & masks[j - nth]) << bit_offsets[j - nth]); \
            break;                                                              \
        case 0: /* TODO : align function check */                               \
            for (j = (nth + num_per_reg - 1); j >= nth; j--)                    \
                packed |= ((luts[j] & masks[j - nth]) << bit_offsets[j - nth]); \
            break;                                                              \
        }                                                                       \
    } while (0)                                                                 \

#define PACK(packed, luts, num_total, num_per_reg, align, masks, bit_offsets)                                                   \
    do {                                                                                                                        \
        int i = 0;                                                                                                              \
        for (i = 0; i < (int)((int)num_total / (int)num_per_reg); i++)                                                          \
            PACK_ONE(packed[i], luts, (i * num_per_reg), num_per_reg, align, masks, bit_offsets);                               \
        if ((int)num_total % (int)num_per_reg)                                                                                  \
            PACK_ONE(packed[i], luts, (i * num_per_reg), ((int)num_total % (int)num_per_reg), align, masks, bit_offsets);       \
    } while (0)                                                                                                                 \

#define HDR_LUT_MAGIC 0xDADADADA

struct hdr_lut_header {
    unsigned int byte_offset;
    unsigned int length;
    unsigned int magic;
    bool operator!=(const struct hdr_lut_header &op) {
        return (this->byte_offset != op.byte_offset) ||
            (this->length != op.length) || (this->magic != op.magic);
    }
    struct hdr_lut_header &operator=(const struct hdr_lut_header &op) {
        this->byte_offset = op.byte_offset;
        this->length = op.length;
        this->magic = op.magic;
        return *this;
    }
};

struct hdr_dat_node {
    struct hdr_lut_header header;
    char *data;
    int group_id;
    hdr_dat_node () {
        this->header.byte_offset = -1;
        this->header.length = -1;
        this->header.magic = -1;
        this->group_id= -1;

        this->data = NULL;
    }
    hdr_dat_node (unsigned int byte_offset, unsigned int length) {
        this->header.byte_offset = byte_offset;
        this->header.length = length;
        this->header.magic = HDR_LUT_MAGIC;
        this->group_id= -1;

        this->data = new char[length*4];
    }
    hdr_dat_node (const struct hdr_dat_node &op) {
        this->header.byte_offset = op.header.byte_offset;
        this->header.length = op.header.length;
        this->header.magic = op.header.magic;
        this->group_id= op.group_id;

        if (op.data != NULL) {
            data = new char[this->header.length*4];
            memcpy(this->data, op.data, this->header.length*4);
        } else 
            this->data = op.data;
    }
    ~hdr_dat_node () {
        this->header.byte_offset = -1;
        this->header.length = -1;
        this->header.magic = -1;
        this->group_id= -1;
        if (this->data != NULL)
            delete[] this->data;
        else
            this->data = NULL;
    }
    struct hdr_dat_node &operator=(const struct hdr_dat_node &op) {
        if (this->header.length != op.header.length) {
            if (this->data != NULL)
                delete[] this->data;
            else
                this->data = NULL;
            this->data = new char[op.header.length*4];
            this->header.length = op.header.length;
        }
        memcpy(&this->header, &op.header, sizeof(struct hdr_lut_header));
        memcpy(this->data, op.data, op.header.length*4);
        this->group_id = op.group_id;
        return *this;
    }
    struct hdr_dat_node &operator=(struct hdr_dat_node &&op) noexcept {
        if (this->header.length != op.header.length) {
            if (this->data != NULL)
                delete[] this->data;
            else
                this->data = NULL;
            this->data = new char[op.header.length*4];
            this->header.length = op.header.length;
        }
        memcpy(&this->header, &op.header, sizeof(struct hdr_lut_header));
        memcpy(this->data, op.data, op.header.length*4);
        this->group_id = op.group_id;
        return *this;
    }
    char *serialize(char *out_data, unsigned int offset, unsigned int length) {
        memcpy(out_data + offset, &this->header, sizeof(struct hdr_lut_header));
        memcpy(out_data + offset + sizeof(struct hdr_lut_header), this->data, length - sizeof(struct hdr_lut_header));
        //memcpy(out_data + offset + sizeof(struct hdr_lut_header) + (this->header.length*4), &this->group_id, sizeof(this->group_id));
        return out_data;
    }
    void set_header (unsigned int byte_offset, unsigned int length) {
        if (this->header.length != length) {
            if (this->data != NULL)
                delete[] this->data;
            else
                this->data = NULL;
            this->data = new char[length*4];
        }
        this->header.byte_offset = byte_offset;
        this->header.length = length;
        this->header.magic = HDR_LUT_MAGIC;
    }
    void set_data (std::vector<int> &lut, 
            int numNodes,
            int numNodesPerReg,
            bool lastNodeAlign,
            std::vector<unsigned int> bitOffsets,
            std::vector<unsigned int> masks) {
        PACK(((int*)this->data), lut, numNodes, numNodesPerReg,
                ((int)lastNodeAlign), masks, bitOffsets);
    }
    void set_group_id (int group_id) {
        this->group_id = group_id;
    }
    void dump_serialized(int level) {
        TAB(level); ALOGD("[header]"); EOL(level);
        TAB(level); ALOGD("offset : 0x%08x", header.byte_offset); EOL(level);
        TAB(level); ALOGD("length : %u", header.length); EOL(level);
        TAB(level); ALOGD("magic : 0x%x", header.magic); EOL(level);
        TAB(level); ALOGD("[data]"); EOL(level);
        TAB(level);
        void *dat = ((char*)this + sizeof(header));
        for (int i = 0; i < header.length; i++) {
            ALOGD("0x%08x ", ((unsigned int*)dat)[i]);
            if ((i + 1) % 10 == 0) {
                ALOGD("|"); EOL(level); TAB(level);
            }
        }
        ALOGD("|"); EOL(level);
    }
    void dump(int level) {
        TAB(level); ALOGD("[header]"); EOL(level);
        TAB(level); ALOGD("offset : 0x%08x", header.byte_offset); EOL(level);
        TAB(level); ALOGD("length : %u", header.length); EOL(level);
        TAB(level); ALOGD("magic : 0x%x", header.magic); EOL(level);
        TAB(level); ALOGD("[data]"); EOL(level);
        TAB(level); ALOGD("[group-id] : %d", this->group_id); EOL(level);
        TAB(level);
        for (int i = 0; i < header.length; i++) {
            ALOGD("0x%08x ", ((unsigned int*)this->data)[i]);
            if ((i + 1) % 10 == 0) {
                ALOGD("|"); EOL(level); TAB(level);
            }
        }
        ALOGD("|"); EOL(level); TAB(level);
    }
    unsigned int size(void) {
        int size = sizeof(this->header);
        size += (this->header.length * sizeof(int));
        return size;
    }
    struct hdr_dat_node *merge(struct hdr_dat_node *op) {
        if (this->header != op->header)
            return NULL;
        if (this->data == NULL)
            return NULL;
        this->header = op->header;

        for (int i = 0; i < this->header.length; i++)
            ((int*)this->data)[i] |= ((int*)op->data)[i];
        return this;
    }
    void queue_and_group(
            std::list<struct hdr_dat_node*> &qList,
            std::unordered_map<int, struct hdr_dat_node> &gMap) {
        if (this->group_id < 0)
            qList.push_back(this);
        else {
            if (gMap.find(this->group_id) != gMap.end())
                gMap[this->group_id].merge(this);
            else {
                struct hdr_dat_node tmp; 
                tmp = (*this);
                gMap[this->group_id] = tmp;
            }
        }
    }
};

struct hdr_coef_header {
    unsigned int total_bytesize;
    union hdr_coef_header_type {
        struct hdr_coef_header_type_unpack {
            unsigned char hw_type; // 0: None, 1: S.LSI, 2: Custom
            unsigned char layer_index;
            unsigned char log_level;
            unsigned char optional_flag;
        } unpack;
        unsigned int pack;
    } type;
    union hdr_coef_header_con {
        struct hdr_coef_header_con_unpack {
            unsigned char mul_en;
            unsigned char mod_en;
            unsigned char reserved[2];
        } unpack;
        unsigned int pack;
    } sfr_con; // uses this for other sfr_con, HDR con would move to lut part
    union hdr_coef_header_lut {
        struct hdr_coef_header_lut_unpack {
            unsigned short shall; // # of shall group
            unsigned short need; // # of need group
        } unpack;
        unsigned int pack;
    } num;
    void init (unsigned int total_bytesize, unsigned int layer_index,
                int log_level, unsigned int mul_en, unsigned int mod_en,
                unsigned int shall, unsigned int need) {
        this->total_bytesize = total_bytesize;
        this->type.unpack.hw_type = 1;
        this->type.unpack.layer_index = layer_index;
        this->type.unpack.log_level = log_level;
        this->type.unpack.optional_flag = 0;

        mul_en = 0;
        this->sfr_con.unpack.mul_en = mul_en;
        this->sfr_con.unpack.mod_en = mod_en;
        this->sfr_con.unpack.reserved[0] = 0;
        this->sfr_con.unpack.reserved[1] = 0;

        this->num.unpack.shall = shall;
        this->num.unpack.need = need;
    }
    void dump(int level) {
        TAB(level); ALOGD("[global header]"); EOL(level);
        TAB(level); ALOGD("total : 0x%08x",           this->total_bytesize); EOL(level);
        TAB(level); ALOGD("hw_type : %u",             this->type.unpack.hw_type); EOL(level);
        TAB(level); ALOGD("layer_index : 0x%x",       this->type.unpack.layer_index); EOL(level);
        TAB(level); ALOGD("log_level : 0x%x",         this->type.unpack.log_level); EOL(level);
        TAB(level); ALOGD("optional : 0x%x",          this->type.unpack.optional_flag); EOL(level);
        TAB(level); ALOGD("mul_en : 0x%x",            this->sfr_con.unpack.mul_en); EOL(level);
        TAB(level); ALOGD("mod_en : 0x%x",            this->sfr_con.unpack.mod_en); EOL(level);
        TAB(level); ALOGD("reserved[0] : 0x%x",       this->sfr_con.unpack.reserved[0]); EOL(level);
        TAB(level); ALOGD("reserved[1] : 0x%x",       this->sfr_con.unpack.reserved[1]); EOL(level);
        TAB(level); ALOGD("shall : 0x%x",     this->num.unpack.shall); EOL(level);
        TAB(level); ALOGD("need : 0x%x",      this->num.unpack.need); EOL(level);
    }
    void dump_serialized(int level) {
        TAB(level); ALOGD("[global header]"); EOL(level);
        TAB(level); ALOGD("total : 0x%08x",           this->total_bytesize); EOL(level);
        TAB(level); ALOGD("hw_type : %u",             this->type.unpack.hw_type); EOL(level);
        TAB(level); ALOGD("layer_index : 0x%x",       this->type.unpack.layer_index); EOL(level);
        TAB(level); ALOGD("log_level : 0x%x",         this->type.unpack.log_level); EOL(level);
        TAB(level); ALOGD("optional : 0x%x",          this->type.unpack.optional_flag); EOL(level);
        TAB(level); ALOGD("mul_en : 0x%x",            this->sfr_con.unpack.mul_en); EOL(level);
        TAB(level); ALOGD("mod_en : 0x%x",            this->sfr_con.unpack.mod_en); EOL(level);
        TAB(level); ALOGD("reserved[0] : 0x%x",       this->sfr_con.unpack.reserved[0]); EOL(level);
        TAB(level); ALOGD("reserved[1] : 0x%x",       this->sfr_con.unpack.reserved[1]); EOL(level);
        TAB(level); ALOGD("shall : 0x%x",     this->num.unpack.shall); EOL(level);
        TAB(level); ALOGD("need : 0x%x",      this->num.unpack.need); EOL(level);
        int offset = sizeof(struct hdr_coef_header);
        level += 1;
        for (int i = 0; i < this->num.unpack.shall; i++) {
            struct hdr_dat_node *dat = (struct hdr_dat_node*)((char*)this + offset);
            offset+= (sizeof(struct hdr_lut_header) + (dat->header.length*4));
            dat->dump_serialized(level);
        }
        for (int i = 0; i < this->num.unpack.need; i++) {
            struct hdr_dat_node *dat = (struct hdr_dat_node*)((char*)this + offset);
            offset+= (sizeof(struct hdr_lut_header) + (dat->header.length*4));
            dat->dump_serialized(level);
        }
    }
};

#endif
