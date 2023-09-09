
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)



#else

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_s16 format

#define VX_TAU 6.28318530717958647692

#ifdef ARM_MALI //mali OpenCL
__kernel void vx_phase(int aw, int ah, int asx, int asy, __global short *a, 
                     int bw, int bh, int bsx, int bsy, __global short *b,
                     int cw, int ch, int csx, int csy, __global uchar *c)

#else
__kernel void vx_phase(int aw, int ah, int asx, int asy, __global uchar *a, 
                     int bw, int bh, int bsx, int bsy, __global uchar *b,
                     int cw, int ch, int csx, int csy, __global uchar *c)
#endif                     
{
    int x = get_global_id(0);
    int y = get_global_id(1);

//	printf("phase : (%d,%d) :(%dx%d),(%dx%d) -> (%dx%d) \n", x, y, aw, ah, bw, bh, cw, ch);


#ifdef ARM_MALI //mali OpenCL
    short in_x = vxImagePixel_short(a, x, y, asx/2, asy/2);
    short in_y = vxImagePixel_short(b, x, y, bsx/2, bsy/2);    

#else
    short in_x = (signed short)vxImagePixel(short, a, x, y, asx, asy);
    short in_y = (signed short)vxImagePixel(short, b, x, y, bsx, bsy);    
#endif

	 /* -M_PI to M_PI */
	 double arct = atan2((double)in_y,(double)in_x);
	 /* 0 - TAU */
	 double norm = arct;
	 if (arct < 0.0f)
	 { 
	 	norm = VX_TAU + arct;
     } 
     /* 0.0 - 1.0 */            
     norm = norm / VX_TAU;            
     
     /* 0 - 255 */            
#ifdef ARM_MALI //mali OpenCL
     vxImagePixel_uchar(c, x, y, csx, csy) = (uchar)((uint)(norm * 256u + 0.5)&0x000000FF);
#else     
     vxImagePixel(uchar, c, x, y, csx, csy) = (uchar)((uint)(norm * 256u + 0.5)&0x000000FF);
#endif     
}

#endif
