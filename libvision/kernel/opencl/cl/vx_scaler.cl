
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)



#else

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format
//Note: we can support only type : VX_INTERPOLATION_TYPE_NEAREST_NEIGHBOR : NearestScaling

__kernel void vx_scaler(int aw, int ah, int asx, int asy, __global uchar *a, 
                     	int bw, int bh, int bsx, int bsy, __global uchar *b,
                    	uint type)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

//    printf("(%d,%d) src(%dx%d)->dst(%dx%d) : type (%d) \n",x,y,aw,ah,bw,bh,type);

	int w1 = aw;	
	int h1 = ah;	
	int w2 = bw;	
	int h2 = bh;	
	
    float wr = (float)w1/(float)w2;    
    float hr = (float)h1/(float)h2;
    
    float x2,y2;

    x2 = (float)x;
    y2 = (float)y;    

    float x_src = ((float)x2+0.5f)*wr - 0.5f;  //get src point according to scale ratio
    float y_src = ((float)y2+0.5f)*hr - 0.5f;

    float x_min = x_src;
    float y_min = y_src;

	int x1 = (int)x_min;			
	int y1 = (int)y_min;			
	if (x_src - x_min >= 0.5f)				
		x1++;			
	if (y_src - y_min >= 0.5f)				
		y1++;

#ifdef ARM_MALI //mali OpenCL
	vxImagePixel_uchar(b, x, y, bsx, bsy) = vxImagePixel_uchar(a, x1, y1, asx, asy);
#else			
	vxImagePixel(uchar, b, x, y, bsx, bsy) = vxImagePixel(uchar, a, x1, y1, asx, asy);
#endif	

}

#endif
