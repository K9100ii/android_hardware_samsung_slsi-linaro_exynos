
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)

__kernel void vx_orr(read_only image2d_t a, read_only image2d_t b, write_only image2d_t c) {
    int2 coord = (get_global_id(0), get_global_id(1));
    write_imageui(c, coord, (read_imageui(a, nearest_clamp, coord) | read_imageui(b, nearest_clamp, coord)));
}

#else

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format

#ifdef ARM_MALI //mali OpenCL
__kernel void vx_orr(int aw, int ah, int asx, int asy, __global uchar *a, 
                     int bw, int bh, int bsx, int bsy, __global uchar *b,
                     int cw, int ch, int csx, int csy, __global uchar *c)

#else
__kernel void vx_orr(int aw, int ah, int asx, int asy, __global void *a, 
                     int bw, int bh, int bsx, int bsy, __global void *b,
                     int cw, int ch, int csx, int csy, __global void *c)
#endif                     
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);
#ifdef ARM_MALI //mali OpenCL
    vxImagePixel_uchar(c, x, y, csx, csy) = vxImagePixel_uchar(a, x, y, asx, asy) | vxImagePixel_uchar(b, x, y, bsx, bsy);
#else    
    vxImagePixel(uchar, c, x, y, csx, csy) = vxImagePixel(uchar, a, x, y, asx, asy) | vxImagePixel(uchar, b, x, y, bsx, bsy);
#endif    
}

#endif
