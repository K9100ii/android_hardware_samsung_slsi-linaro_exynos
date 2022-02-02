
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)



#else

__kernel void vx_meanstddev(int aw, int ah, int asx, int asy, __global uchar *a, 
		                     int mean, int stddev)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    
//    vxImagePixel(uchar, c, x, y, csx, csy) = vxImagePixel(uchar, a, x, y, asx, asy) ^ vxImagePixel(uchar, b, x, y, bsx, bsy);
}

#endif
