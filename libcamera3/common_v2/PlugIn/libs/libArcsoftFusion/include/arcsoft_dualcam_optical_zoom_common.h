/*----------------------------------------------------------------------------------------------
*
* This file is ArcSoft's property. It contains ArcSoft's trade secret, proprietary and         
* confidential information. 
* 
* The information and code contained in this file is only for authorized ArcSoft employees 
* to design, create, modify, or review.
* 
* DO NOT DISTRIBUTE, DO NOT DUPLICATE OR TRANSMIT IN ANY FORM WITHOUT PROPER AUTHORIZATION.
* 
* If you are not an intended recipient of this file, you must not copy, distribute, modify, 
* or take any action in reliance on it. 
* 
* If you have received this file in error, please immediately notify ArcSoft and 
* permanently delete the original and any copy of any file and any printout thereof.
*
*-------------------------------------------------------------------------------------------------*/

#ifndef __ARCSOFT_DUALCAM_OPTICAL_ZOOM_COMMON_H__20161003_
#define __ARCSOFT_DUALCAM_OPTICAL_ZOOM_COMMON_H__20161003_

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************************************
* Function name : ARC_DCOZ_FNPROGRESS
* Description   : Call back function for updating the progress 
**************************************************************************************************/
typedef MRESULT (*ARC_DCOZ_FNPROGRESS)(
        MInt32 i32Percent,     // The percentage of the current operation.
        MVoid *pUserData       // Caller-defined data.
        );

#define ARC_DCOZ_CALIBRATIONDATA_LEN  2048
typedef struct _tagARC_DCOZ_INITPARAM
{
    MFloat                fWideFov;        //[in] Fov value of wide camera.
    MFloat                fTeleFov;       //[in] Fov value of tele camera.
    MByte*               pCalData;        //[in] Calibration data.
    MInt32                i32CalDataLen;  //[in] MUST be ARC_DCOZ_CALIBRATIONDATA_LEN.
    ARC_DCOZ_FNPROGRESS    fnCallback;    //[in] Call back function for updating the progress.
    MVoid*                    pUserData;  //[in] Caller-defined data of fnCallback.
}ARC_DCOZ_INITPARAM, *LPARC_DCOZ_INITPARAM;

#define ARC_DCOZ_WIDECAMERAOPENED         (0x4)
#define ARC_DCOZ_TELECAMERAOPENED         (0x8)

#define ARC_DCOZ_WIDECAMERASHOULDOPEN         (0x1)
#define ARC_DCOZ_TELECAMERASHOULDOPEN        (0x2)


#define ARC_DCOZ_RECOMMENDMASK (0x30)

#define ARC_DCOZ_RECOMMENDDEFAULT (0x00)
#define ARC_DCOZ_RECOMMENDTOTELE (0x10)
#define ARC_DCOZ_RECOMMENDFB (0x20)
#define ARC_DCOZ_RECOMMENDTOWIDE (0x30)

#define ARC_DCOZ_MASTERSESSIONMASK (0x80)
#define ARC_DCOZ_WIDESESSION (0x00)
#define ARC_DCOZ_TELESESSION (0x80)

#define ARC_DCOZ_SWITCHMASK (0x100)
#define ARC_DCOZ_SWITCHTOWIDE (0x100)
#define ARC_DCOZ_SWITCHTOTELE (0x000)

//for ARC_DCOZ_MODE_NOMARGIN mode
#define ARC_DCOZ_BYPASSFLAGMASK (0x200)
#define ARC_DCOZ_NORMAL         (0x200)
#define ARC_DCOZ_BAYASS         (0x000)



#define ARC_DCOZ_CAMERANEEDFLAGMASK      (0x30000)
#define ARC_DCOZ_CAMERANEEDFLAGDEFAULT (0x00000)
#define ARC_DCOZ_NEEDWIDEFRAMEONLY      (0x10000)
#define ARC_DCOZ_NEEDTELEFRAMEONLY      (0x20000)
#define ARC_DCOZ_NEEDBOTHFRAMES      (0x30000)

#define ARC_DCOZ_BUTTON2XFLAG	  (0x80000)

typedef struct _tagARC_DCOZ_Fraction {
    unsigned int den; /* denominator */
    union
    {
        unsigned int num; /* unsigned numerator */
        int snum; /* signed numerator */
    };
} ARC_DCOZ_Fraction;


typedef struct
{
    MInt32 i32FdPlane1;
    MInt32 i32FdPlane2;
    MInt32 i32FdPlane3;
    MInt32 i32FdPlane4;
}ARC_DCOZ_IMAGEFD, *LPARC_DCOZ_IMAGEFD;

typedef struct
{
    MInt32 i32Top;
    MInt32 i32Left;
    MInt32 i32Right;
    MInt32 i32Bottom;
}ARC_DCOZ_RECT, *LPARC_DCOZ_RECT;


typedef struct
{
    MInt32 i32FaceNum;
    ARC_DCOZ_RECT *pFaceRect;
//	MInt32 * pFaceOrient;
}ARC_DCOZ_FACEINFO, *LPARC_DCOZ_FACEINFO;


#ifdef __cplusplus
}
#endif

#endif //__ARCSOFT_DUALCAM_OPTICAL_ZOOM_COMMON_H__20161003_


