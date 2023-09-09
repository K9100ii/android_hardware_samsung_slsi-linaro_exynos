
#include <vx_cl.h>

#if defined(CL_USE_IMAGES)

#else

#if 0 //jspark : opencl can not support recursive call in function 
static void quicksort(uchar a[], int lo, int hi)
{
    //lo : is the lower index, hi : is the upper index
	int i=lo ,j=hi, h;
	uchar x = a[(lo+hi)/2];

	//partition
	do {
		while(a[i]<x) i++;
		while(a[j]>x) j--;	
		if (i<=j) {
			h = a[i];
			a[i] = a[j];
			a[j] = h;
			i++;
			j--;
			}
	}while(i<=j);

	//recursion
	if (lo < j) quicksort(a,lo,j);
	if (i < hi) quicksort(a,i,hi);	
}
#endif

static void bubble_sort(uchar a[], int length)
{
	int i,j;
	uchar temp;
	for (i = length-1; i >= 0; i--)
	{
		for (j = 1; j <= i; j++)
		{
			if (a[j-1] > a[j]) 
			{
				temp = a[j-1];
				a[j-1] = a[j];
				a[j] = temp;
			}
		}
	}
}

//[2016/03/09] jongseok,Park
//this kernel assume it's only for image_u8 format (we have to modify bubble sort for performance later)

#ifdef ARM_MALI //mali OpenCL
__kernel void vx_median3x3(int aw, int ah, int asx, int asy, __global uchar *a,
                             int bw, int bh, int bsx, int bsy, __global uchar *b)
#else
__kernel void vx_median3x3(int aw, int ah, int asx, int asy, __global void *a,
                             int bw, int bh, int bsx, int bsy, __global void *b)
#endif                             
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    const size_t w = get_global_size(0);
    const size_t h = get_global_size(1);
    uchar pixels[9];
    uchar out;
    int i;
    
 //   printf("median3x3 : (%d,%d) src(%dx%d)->dst(%dx%d) \n",x,y,aw,ah,bw,bh);

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

	//quicksort(pixels,0,8);
    bubble_sort(pixels,9);

    out = pixels[4]; //center of value

#ifdef ARM_MALI //mali OpenCL
    vxImagePixel_uchar(b, x, y, bsx, bsy) = out;
#else                
    vxImagePixel(uchar, b, x, y, bsx, bsy) = out;
#endif    
}

#endif
