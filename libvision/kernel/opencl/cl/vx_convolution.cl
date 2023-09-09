
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)


#else

#define VX_INT_MAX_CONVOLUTION_DIM (15)
#define INT16_MIN        -32767
#define INT16_MAX        32767

#ifdef ARM_MALI //mali OpenCL
__kernel void vx_convolution(	int aw, int ah, int asx, int asy, __global uchar *a, 
								uint conv_width, uint conv_height, uint scale,  __global signed short *conv_mat,
								int bw, int bh, int bsx, int bsy, __global short *b)

#else
__kernel void vx_convolution(	int aw, int ah, int asx, int asy, __global void *a, 
								uint conv_width, uint conv_height, uint scale,  __global void *m,
								int bw, int bh, int bsx, int bsy, __global void *b)
#endif								
                     
{
	if ((conv_width > VX_INT_MAX_CONVOLUTION_DIM) || (conv_height > VX_INT_MAX_CONVOLUTION_DIM))
		return;

    int x = get_global_id(0);
    int y = get_global_id(1);
    
#ifdef ARM_MALI //mali OpenCL
    const int w = get_global_size(0);
    const int h = get_global_size(1);

#else    
    const size_t w = get_global_size(0);
    const size_t h = get_global_size(1);
#endif    

    int low_x, low_y, high_x, high_y;
    int conv_radius_x, conv_radius_y;
    int i,j,k=0;
    int sum = 0, value = 0;
    
#ifdef ARM_MALI //mali OpenCL
    uchar pixels[VX_INT_MAX_CONVOLUTION_DIM*VX_INT_MAX_CONVOLUTION_DIM];
#else
    local uchar pixels[VX_INT_MAX_CONVOLUTION_DIM*VX_INT_MAX_CONVOLUTION_DIM];
#endif    
        
    conv_radius_x = conv_width / 2;
    conv_radius_y = conv_height / 2;
    
    low_x = conv_radius_x;      
    high_x = ((w >= conv_radius_x) ? w-conv_radius_x : 0);
    low_y = conv_radius_y;
    high_y = ((h >= conv_radius_y) ? h-conv_radius_y : 0);

#ifdef ARM_MALI //mali OpenCL
#else
	signed short *conv_mat = (signed short *)m;
#endif	
   
//	printf("(conv)[%d,%d] %dx%d, (low(%d,%d), high(%d,%d), scale:%d with marix (%d, %d, %d / %d, %d, %d / %d, %d, %d)\n", 
//												x,y, conv_width, conv_height, low_x,high_x,low_y,high_y,scale,
//												conv_mat[0], conv_mat[1], conv_mat[2],	conv_mat[3], conv_mat[4], conv_mat[5],	conv_mat[6], conv_mat[7], conv_mat[8] );

	//boarder mode	
	if ((x<low_x) || (y<low_y) || (x>=high_x) || (y>=high_y))
		return; 
		
	//reading pixels
	for (j = -conv_radius_y; j <= conv_radius_y; j++) {
		for (i = -conv_radius_x; i <= conv_radius_x; i++) {
#ifdef ARM_MALI //mali OpenCL
			pixels[k++] = vxImagePixel_uchar(a, x+i, y+j, asx, asy);
#else		
			pixels[k++] = vxImagePixel(uchar, a, x+i, y+j, asx, asy);
#endif			
	    }
	}
	//convolution	
    for (i = 0; i < conv_width * conv_height; ++i)
    	sum += conv_mat[conv_width * conv_height - 1 - i] * pixels[i];

    value = sum  / (int) scale;

    if (value < INT16_MIN) 
	   value = INT16_MIN;  
    else if (value > INT16_MAX) 
       value = INT16_MAX;
       
	//writing result of convolution      
#ifdef ARM_MALI //mali OpenCL
    vxImagePixel_short(b, x, y, bsx/2, bsy/2) = (signed short)value;
#else
    vxImagePixel(signed short, b, x, y, bsx, bsy) = (signed short)value;
#endif    
}

#endif




#if 0 //from GitHub
__kernel  void convolution( 
     __read_only image2d_t inputImage, 
     __write_only image2d_t outputImage, 
     int imageWidth, 
     int imageHeight, 
     __constant float* filter, 
     int filterSize, 
     sampler_t sampler 
     ) 
 { 
 	int x = get_global_id(0); 
     int y = get_global_id(1); 
      
     int halfSize= filterSize>>1; 
     int2 coord; 
      
     float4 sum = {0.f, 0.f, 0.f, 0.f}; 
     int filterIdx = 0; 
     for (int j = -halfSize; j <= halfSize; ++j) { 
         coord.y = y + j; 
         for (int i = -halfSize; i <= halfSize; ++i) { 
             coord.x = x + i; 
 
 
             float4 pixel = read_imagef(inputImage, sampler, coord); 
 
 
             sum.x += pixel.x * filter[filterIdx++]; 
         } 
     } 
 

     write_imagef(outputImage, (int2) (x, y), sum); 
 } 
 #endif
