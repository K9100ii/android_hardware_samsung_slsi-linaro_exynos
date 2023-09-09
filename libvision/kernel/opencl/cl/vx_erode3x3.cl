
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)



#else

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format

static uchar min_op(uchar a, uchar b) 
{    
	return a < b ? a : b;
}


#ifdef ARM_MALI //mali OpenCL	
__kernel void vx_erode3x3(int aw, int ah, int asx, int asy, __global uchar *a, 
                     int bw, int bh, int bsx, int bsy, __global uchar *b)
#else
__kernel void vx_erode3x3(int aw, int ah, int asx, int asy, __global void *a, 
                     int bw, int bh, int bsx, int bsy, __global void *b)
#endif                     
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    const size_t w = get_global_size(0);
    const size_t h = get_global_size(1);

#ifdef ARM_MALI //mali OpenCL 
    uchar pixels[9];
#else
    local uchar pixels[9];
#endif    
    uchar out;
    int i;
    
 //   printf("erode : (%d,%d) src(%dx%d)->dst(%dx%d) \n",x,y,aw,ah,bw,bh);

   if (y == 0 || x == 0 || x == (w - 1) || y == (h - 1))
       return;

#ifdef ARM_MALI //mali OpenCL
    pixels[0] = vxImagePixel_uchar(a, x-1, y-1, asx, asy);
    pixels[1] = vxImagePixel_uchar(a, x+0, y-1, asx, asy);
    pixels[2] = vxImagePixel_uchar(a, x+1, y-1, asx, asy);
    pixels[3] = vxImagePixel_uchar(a, x-1, y+0, asx, asy);
    pixels[4] = vxImagePixel_uchar(a, x+0, y+0, asx, asy);
    pixels[5] = vxImagePixel_uchar(a, x+1, y+0, asx, asy);
    pixels[6] = vxImagePixel_uchar(a, x-1, y+1, asx, asy);
    pixels[7] = vxImagePixel_uchar(a, x+0, y+1, asx, asy);
    pixels[8] = vxImagePixel_uchar(a, x+1, y+1, asx, asy);
#else       
    pixels[0] = vxImagePixel(uchar, a, x-1, y-1, asx, asy);
    pixels[1] = vxImagePixel(uchar, a, x+0, y-1, asx, asy);
    pixels[2] = vxImagePixel(uchar, a, x+1, y-1, asx, asy);
    pixels[3] = vxImagePixel(uchar, a, x-1, y+0, asx, asy);
    pixels[4] = vxImagePixel(uchar, a, x+0, y+0, asx, asy);
    pixels[5] = vxImagePixel(uchar, a, x+1, y+0, asx, asy);
    pixels[6] = vxImagePixel(uchar, a, x-1, y+1, asx, asy);
    pixels[7] = vxImagePixel(uchar, a, x+0, y+1, asx, asy);
    pixels[8] = vxImagePixel(uchar, a, x+1, y+1, asx, asy);
#endif

    out = pixels[0];
    for (i = 1; i < 9; i++)  {
       out = min_op(out, pixels[i]);   //erode
    	}

#ifdef ARM_MALI //mali OpenCL
    vxImagePixel_uchar(b, x, y, bsx, bsy) = out;
#else                
    vxImagePixel(uchar, b, x, y, bsx, bsy) = out;
#endif    
}

#endif
