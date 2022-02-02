
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)

__kernel void vx_xor(read_only image2d_t a, read_only image2d_t b, write_only image2d_t c) {
    int2 coord = (get_global_id(0), get_global_id(1));
    write_imageui(c, coord, (read_imageui(a, nearest_clamp, coord) ^ read_imageui(b, nearest_clamp, coord)));
}

#else

__kernel void vx_xor(int aw, int ah, int asx, int asy, __global uchar *a, 
                     int bw, int bh, int bsx, int bsy, __global uchar *b,
                     int cw, int ch, int csx, int csy, __global uchar *c)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    
#ifdef ARM_MALI //mali OpenCL
    //vxImagePixel_uchar(c, x, y, csx, csy) = vxImagePixel_uchar(a, x, y, asx, asy) ^ vxImagePixel_uchar(b, x, y, bsx, bsy);

		uchar p0 = vxImagePixel_uchar(a, x, y, asx, asy);
		uchar p1 = vxImagePixel_uchar(b, x, y, bsx, bsy);
		uchar p2 = p1 ^ p0;
    vxImagePixel_uchar(c, x, y, csx, csy) = p2;

    //if (x==10)
    //{
    //	printf("[XOR] (%d,%d): aw:%d,ah:%d,asx:%d,asy:%d (%d,%d[%x,%x] -> %d)\n",x,y, aw,ah,asx,asy, p0,p1,p0,p1,p2);
    //}

        
#else    
    vxImagePixel(uchar, c, x, y, csx, csy) = vxImagePixel(uchar, a, x, y, asx, asy) ^ vxImagePixel(uchar, b, x, y, bsx, bsy);
#endif    
}

#endif
