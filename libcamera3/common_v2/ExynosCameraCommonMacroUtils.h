#ifndef EXYNOS_CAMERA_COMMON_MACRO_UTILS_H__
#define EXYNOS_CAMERA_COMMON_MACRO_UTILS_H__

/* ---------------------------------------------------------- */
/* Align */
#define ROUND_UP(x, a)              (((x) + ((a)-1)) / (a) * (a))
#define ROUND_OFF_HALF(x, dig)      ((float)(floor((x) * pow(10.0f, dig) + 0.5) / pow(10.0f, dig)))
#define ROUND_OFF_DIGIT(x, dig)     ((uint32_t)(floor(((double)x)/((double)dig) + 0.5f) * dig))

/* Image processing */
#define SATURATING_ADD(a, b)  (((a) > (0x3FF - (b))) ? 0x3FF : ((a) + (b)))
#define COMBINE(a, b) ((((a<<20)&0xFFF00000)|((b<<8)&0x000FFF00)))
#define COMBINE_P0(a, b) ((((a)&0x00FF)|((b<<8)&0x0F00)))
#define COMBINE_P1(a, b) ((((a>>4)&0x000F)|((b<<4)&0x0FF0)))
#define COMBINE_P3(a, b) ((((a>>8)&0x000F)|((b<<4)&0x00F0)))
#define COMBINE_P4(a, b) ((((a<<8)&0x0F00)|((b)&0x00FF)))

/* ---------------------------------------------------------- */
#define SIZE_RATIO(w, h)         ((w) * 10 / (h))
#define MAX(x, y)                (((x)<(y))? (y) : (x))

#endif /* EXYNOS_CAMERA_COMMON_MACRO_UTILS_H__ */

