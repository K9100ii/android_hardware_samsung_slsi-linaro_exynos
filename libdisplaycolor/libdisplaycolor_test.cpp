#include<hardware/exynos/libdisplaycolor.h>
#include<stdio.h>
#include<stdlib.h>
#include<string>

using std::vector;
using std::string;

#define DQE_COLORMODE_MAGIC     0xDA
struct dqe_colormode_header {
    uint8_t magic;
    uint8_t seq_num;
    uint16_t total_size;
    uint16_t header_size;
    uint16_t num_data;
    uint32_t reserved;
};
struct dqe_colormode_data {
    uint8_t magic;
    uint8_t id;
    uint16_t total_size;
    uint16_t header_size;
    uint8_t attr[4];
    uint16_t reserved;
};
enum dqe_colormode_node_id {
    DQE_COLORMODE_ID_CGC17_ENC = 1,
    DQE_COLORMODE_ID_CGC17_CON,
    DQE_COLORMODE_ID_CGC_DITHER,
    DQE_COLORMODE_ID_DEGAMMA,
    DQE_COLORMODE_ID_GAMMA,
    DQE_COLORMODE_ID_GAMMA_MATRIX,
    DQE_COLORMODE_ID_HSC48_LCG,
    DQE_COLORMODE_ID_HSC,
    DQE_COLORMODE_ID_SCL,
    DQE_COLORMODE_ID_MAX,
};

int main (int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage : <executable> <mode> <intent>\n");
        return -1;
    }
    string arg1 = argv[1];
    string arg2 = argv[2];
    int mode = stoi(arg1);
    int intent = stoi(arg2);
    char *parcel;
    int parcel_size;

    class IDisplayColor *dispColorIF = IDisplayColor::createInstance(0);
    vector<struct DisplayColorMode> CMList;
    vector<struct DisplayRenderIntent> RIList;
    CMList = dispColorIF->getColorModes();
    printf("[supported color modes]\n");
    for (int i = 0; i < CMList.size(); i++) {
        printf("(%d: modeId(%d), gamutId(%d), modeName(%s))\n",
                i, CMList[i].modeId, CMList[i].gamutId, CMList[i].modeName.c_str());
        RIList = dispColorIF->getRenderIntents(CMList[i].modeId);
        printf("---[supported render intent]\n");
        for (int j = 0; j < RIList.size(); j++)
            printf("---(%d: intentId(%d), intentName(%s))\n",
                    j, RIList[j].intentId, RIList[j].intentName.c_str());
    }
    dispColorIF->setColorModeWithRenderIntent(mode, intent);

    parcel_size = dispColorIF->getDqeLutSize();
    printf("parcel_size(%d)\n", parcel_size);
    parcel = (char*)malloc(parcel_size);
    printf("--------------------------------------------------\n");
    dispColorIF->getDqeLut(parcel);

    /* verify parcel */
    struct dqe_colormode_header *g_header = (struct dqe_colormode_header *)parcel;
    struct dqe_colormode_data * d_header = (struct dqe_colormode_data *)(parcel + sizeof(struct dqe_colormode_header));
    char *d_ = (char*)(parcel + sizeof(struct dqe_colormode_header) + sizeof(struct dqe_colormode_data));
    printf("[header]\n");
    printf("magic(0x%x)\n", g_header->magic);
    printf("seq_num(%d)\n", g_header->seq_num);
    printf("total_size(%d)\n", g_header->total_size);
    printf("header_size(%d)\n", g_header->header_size);
    printf("num_data(%d)\n", g_header->num_data);
    for (int i = 0; i < g_header->num_data; i++) {
        if (i != 0) {
            int total_size = d_header->total_size;
            d_header = (struct dqe_colormode_data *)((char*)d_header + total_size);
            d_ = (char*)d_ + total_size;
        }
        printf("[data_header%d]\n", i);
        printf("magic(0x%x)\n",         d_header->magic);
        printf("seq_num(%d)\n",         d_header->id);
        printf("total_size(%d)\n",      d_header->total_size);
        printf("header_size(%d)\n",     d_header->header_size);
        printf("attr0(%d)\n",        d_header->attr[0]);
        printf("attr1(%d)\n",        d_header->attr[1]);
        printf("attr2(%d)\n",        d_header->attr[2]);
        printf("attr3(%d)\n",        d_header->attr[3]);
        printf("[data%d]\n", i);
        printf("%s\n", d_);
    }

    printf("--------------------------------------------------\n");
    float matrix[16] =
    {1.0, 0.0, 0.0, 0.0,
    1.0, 1.0, 0.0, 0.0,
    1.0, 0.0, 1.0, 0.0,
    1.0, 0.0, 0.0, 1.0};

    dispColorIF->setColorTransform(matrix, 0);
    dispColorIF->getDqeLut(parcel);

    g_header = (struct dqe_colormode_header *)parcel;
    d_header = (struct dqe_colormode_data *)(parcel + sizeof(struct dqe_colormode_header));
    d_ = (char*)(parcel + sizeof(struct dqe_colormode_header) + sizeof(struct dqe_colormode_data));
    printf("[header]\n");
    printf("magic(0x%x)\n", g_header->magic);
    printf("seq_num(%d)\n", g_header->seq_num);
    printf("total_size(%d)\n", g_header->total_size);
    printf("header_size(%d)\n", g_header->header_size);
    printf("num_data(%d)\n", g_header->num_data);
    for (int i = 0; i < g_header->num_data; i++) {
        if (i != 0) {
            int total_size = d_header->total_size;
            d_header = (struct dqe_colormode_data *)((char*)d_header + total_size);
            d_ = (char*)d_ + total_size;
        }
        printf("[data_header%d]\n", i);
        printf("magic(0x%x)\n",         d_header->magic);
        printf("id(%d)\n",              d_header->id);
        printf("total_size(%d)\n",      d_header->total_size);
        printf("header_size(%d)\n",     d_header->header_size);
        printf("attr0(%d)\n",        d_header->attr[0]);
        printf("attr1(%d)\n",        d_header->attr[1]);
        printf("attr2(%d)\n",        d_header->attr[2]);
        printf("attr3(%d)\n",        d_header->attr[3]);
        printf("[data%d]\n", i);
        printf("%s\n", d_);
    }

    printf("--------------------------------------------------\n");
    dispColorIF->setColorMode(mode);
    dispColorIF->setColorTransform(matrix, 0);
    dispColorIF->getDqeLut(parcel);
/*
    g_header = (struct dqe_colormode_header *)parcel;
    d_header = (struct dqe_colormode_data *)(parcel + sizeof(struct dqe_colormode_header));
    d_ = (char*)(parcel + sizeof(struct dqe_colormode_header) + sizeof(struct dqe_colormode_data));
    printf("[header]\n");
    printf("magic(0x%x)\n", g_header->magic);
    printf("seq_num(%d)\n", g_header->seq_num);
    printf("total_size(%d)\n", g_header->total_size);
    printf("header_size(%d)\n", g_header->header_size);
    printf("num_data(%d)\n", g_header->num_data);
    for (int i = 0; i < g_header->num_data; i++) {
        if (i != 0) {
            int total_size = d_header->total_size;
            d_header = (struct dqe_colormode_data *)((char*)d_header + total_size);
            d_ = (char*)d_ + total_size;
        }
        printf("[data_header%d]\n", i);
        printf("magic(0x%x)\n",         d_header->magic);
        printf("seq_num(%d)\n",         d_header->id);
        printf("total_size(%d)\n",      d_header->total_size);
        printf("header_size(%d)\n",     d_header->header_size);
        printf("attr0(%d)\n",        d_header->attr[0]);
        printf("attr1(%d)\n",        d_header->attr[1]);
        printf("attr2(%d)\n",        d_header->attr[2]);
        printf("attr3(%d)\n",        d_header->attr[3]);
        printf("[data%d]\n", i);
        printf("%s\n", d_);
    }
*/
    for (int i = 0; i < parcel_size; i++) {
        if (i % 40 == 0)
            printf("\n");
        printf("0x%2x,", parcel[i]);
    }
    printf("\n");
    return 0;
}
