
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)



#else
//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format

__kernel void vx_integral(int aw, int ah, int asx, int asy, __global uchar *a, 
                     int bw, int bh, int bsx, int bsy, __global uchar *b)

{
    int x = get_global_id(0);
    int y = get_global_id(1);
    
//    vxImagePixel(uchar, c, x, y, csx, csy) = vxImagePixel(uchar, a, x, y, asx, asy) ^ vxImagePixel(uchar, b, x, y, bsx, bsy);
}

#endif
