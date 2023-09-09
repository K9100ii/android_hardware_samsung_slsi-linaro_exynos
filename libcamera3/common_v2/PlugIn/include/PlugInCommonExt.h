#ifndef PLUG_IN_COMMON_EXT_H
#define PLUG_IN_COMMON_EXT_H

typedef struct {
    int x;
    int y;
    int z;
    float x_bias;
    float y_bias;
    float z_bias;
    uint64_t timestamp;
}plugin_gyro_data_t;

typedef plugin_gyro_data_t* Array_pointer_gyro_data_t;

enum PLUGIN_PARAMETER_KEY {
    PLUGIN_PARAMETER_KEY_START = 0,
    PLUGIN_PARAMETER_KEY_STOP = 1,
};

typedef struct {
    int x;
    int y;
    int w;
    int h;
}plugin_rect_t;

typedef plugin_rect_t* Data_pointer_rect_t;

#endif //PLUG_IN_COMMON_EXT_H
