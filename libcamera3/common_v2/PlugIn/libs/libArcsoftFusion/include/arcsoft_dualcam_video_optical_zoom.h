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

#ifndef __ARCSOFT_DUALCAM_VIDEO_OPTICAL_ZOOM_H__20161003_
#define __ARCSOFT_DUALCAM_VIDEO_OPTICAL_ZOOM_H__20161003_

#include "amcomdef.h"
#include "asvloffscreen.h"
#include "merror.h"
#include "ammem.h"
#include "arcsoft_dualcam_optical_zoom_common.h"

#ifdef DUALCAMVIDEOOPTICALZOOMDLL_EXPORTS
#define ARC_DCVOZ_API __declspec(dllexport)
#else
#define ARC_DCVOZ_API
#endif

#ifdef __cplusplus
extern "C" {
#endif


typedef struct _tagARC_DCVOZ_RATIOPARAM
{
    MFloat fWideViewRatio;                      //[in] The zoom level of user image while getting wide image.
    MFloat fWideImageRatio;                     //[in] The zoom level of wide image.
    MFloat fTeleViewRatio;                      //[in] The zoom level of user image while getting tele image.
    MFloat fTeleImageRatio;                     //[in] The zoom level of tele image.
    MInt32  bNeedWideFullBuffer;                //[in] If use full buffer or not.
    MInt32  bNeedTeleFullBuffer;                //[in] If use full buffer or not.
}ARC_DCVOZ_RATIOPARAM, *LPARC_DCVOZ_RATIOPARAM;


typedef struct _tagARC_DCVOZ_CAMERAPARAM
{
    MInt32 ISOWide;                   // [in] ISO of wide camera.
    MInt32 ISOTele;                   // [in] ISO of tele camera.
    ARC_DCOZ_Fraction EVWide;                    // [in] Exposure value of wide camera
    ARC_DCOZ_Fraction EVTele;                    // [in] Exposure value of tele camera
    ARC_DCOZ_Fraction exposureTimeWide;          // [in] Exposure time of wide camera
    ARC_DCOZ_Fraction exposureTimeTele;          // [in] Exposure time of tele camera
    MInt32 Orientation;                          // [in] Device orientation
    MInt32 CaptureStatus;                        // [in] Capture status;
    MInt32 WideFocusFlag;           // [in] Wide finish the automatic focus or not.
    MInt32 TeleFocusFlag;           // [in] Tele finish the automatic focus or not.
    MInt32 CameraControlFlag;       // [in/out] camera system feedback Wide Camera Opened:[(CameraControlFlag&ARC_DCOZ_WIDECAMERAOPENED)==1].
                                    //camera system feedback Wide Camera Closed:[(CameraControlFlag&ARC_DCOZ_WIDECAMERAOPENED)==0].
                                    //camera system feedback Tele Camera Opened:[(CameraControlFlag&ARC_DCOZ_TELECAMERAOPENED)==1].
                                    //camera system feedback Tele Camera Closed:[(CameraControlFlag&ARC_DCOZ_TELECAMERAOPENED)==0].
                                    //algorithm recommend switch to wide:[(CameraControlFlag&ARC_DCOZ_SWITCHMASK)==ARC_DCOZ_SWITCHTOWIDE].
                                    //algorithm recommend switch to tele:[(CameraControlFlag&ARC_DCOZ_SWITCHMASK)==ARC_DCOZ_SWITCHTOTELE].
      
    ARC_DCOZ_RECT WideFocusRoi;        //[in] wide camera focus roi
    ARC_DCOZ_RECT TeleFocusRoi;        //[in] tele camera focus roi
    MFloat WideObjectDistanceValue;    //[in] Focus position of wide camera
    MFloat TeleObjectDistanceValue;    //[in] Focus position of tele camera
    MUInt64 TimestampWide;            //[in] Wide time stamp.
    MUInt64 TimestampTele;            //[in] Tele time stamp.

    MInt32 CameraIndex;               // [out]    Result image is base on wide or tele camera. The wide is ARC_DCOZ_BASEWIDE. The tele is ARC_DCOZ_BASETELE.
    MInt32 ImageShiftX;               // [out]    Capture image x offset
    MInt32 ImageShiftY;               // [out]    Capture image y offset
    MInt32 PreviewImageShiftX;        // [out]    Preview image x offset
    MInt32 PreviewImageShiftY;        // [out]    Preview image y offset
    MRECT  CropRectForCPP;            //[out] the CropRect for CPP (Qualcomm)

}ARC_DCVOZ_CAMERAPARAM, *LPARC_DCVOZ_CAMERAPARAM;

typedef struct _tagARC_DCVOZ_CAMERAIMAGEINFO
{
    MInt32        i32WideFullWidth;    //[in] Width of wide full image by ISP.
    MInt32        i32WideFullHeight;   //[in] Height of wide full image by ISP.
    MInt32        i32TeleFullWidth;    //[in] Width of tele full image by ISP.
    MInt32        i32TeleFullHeight;   //[in] Height of tele full image by ISP.
    MInt32        i32WideWidth;        //[in/out]    Input width of wide image and output the max width of wide image while resized.
    MInt32        i32WideHeight;       //[in/out]    Input height of wide image and output the max height of wide image while resized.
    MInt32        i32TeleWidth;        //[in/out]    Input width of tele image and output the max width of tele image while resized.
    MInt32        i32TeleHeight;       //[in/out]    Input height of tele image and output the max height of tele image while resized.
    MInt32        i32DstWidth;         //[in/out] Width of result image by algorithm.
    MInt32        i32DstHeight;        //[in/out] Height of result image by algorithm.
}ARC_DCVOZ_CAMERAIMAGEINFO, *LPARC_DCVOZ_CAMERAIMAGEINFO;


typedef struct _tagARC_DCVOZ_RECORDING_PARAM
{
	MInt32 CameraIndex;   //[in/out]  Result image is base on wide or tele camera. The wide is ARC_DCOZ_BASEWIDE. The tele is ARC_DCOZ_BASETELE.
	MFloat WParameter_1;  //[in/out]  Parameter for preview and record sync.
	MFloat WParameter_2;  //[in/out]  Parameter for preview and record sync.
	MFloat TParameter_1;  //[in/out]  Parameter for preview and record sync.
	MFloat TParameter_2;  //[in/out]  Parameter for preview and record sync.
	MFloat Parameter_1;   //[in/out]  Parameter for preview and record sync.
	MFloat Parameter_2;   //[in/out]  Parameter for preview and record sync.
}ARC_DCVOZ_RECORDING_PARAM, *LPARC_DCVOZ_RECORDING_PARAM;


/** @brief Get version information of the algorithm.
* 
* @return            The version information 
*/
ARC_DCVOZ_API const MPBASE_Version *ARC_DCVOZ_GetVersion();


/** @brief Initialize the algorithm engine.
* 
* @param[in/out]    phHandle        Handle of the algorithm engine.
* @param[in]        pInitParam        The initial parameter.
* @return            MOK if success, otherwise fail.
*/
ARC_DCVOZ_API MRESULT ARC_DCVOZ_Init(
        MHandle*             phHandle,
        LPARC_DCOZ_INITPARAM pInitParam
        );

/** @brief
* @param[in]        hHandle                        Handle of the algorithm engine.
* @param[in/out]    pCameraImageInfo            Image information.
* @param[in]        i32Mode                        The mode of the algorithm.
* @return            MOK if success, otherwise fail.
*/
#define ARC_DCOZ_MODE_A     4  //for Preview, Resize input image with zoom. Output target size.
#define ARC_DCOZ_MODE_F 	8  //for Preview &Recording with & without VDIS (in W ,out if <6x W*1.2, if >6=6x 1.3*W))
#define ARC_DCOZ_MODE_NOMARGIN    10 //for Preview, No margin for 4K case.
ARC_DCVOZ_API MRESULT ARC_DCVOZ_SetPreviewSize(MHandle        hHandle,
                                               LPARC_DCVOZ_CAMERAIMAGEINFO    pCameraImageInfo,
                                               MInt32        i32Mode);
/** @brief Uninitialize the algorithm engine
* 
* @param[in/out]    hHandle        Handle of the algorithm engine.
* @return            MOK if success, otherwise fail.
*/
ARC_DCVOZ_API MRESULT ARC_DCVOZ_Uninit(MHandle* phHandle);

/** @brief Set the zoom list of camera.
* @param[in]        hHandle                     Handle of the algorithm engine.
* @param[in]        pi32WideZoomList            The zoom list of wide camera.
* @param[in]        i32WideZoomListSize         The size of the zoom list.
* @param[in]        pi32TeleZoomList            The zoom list of tele camera.
* @param[in]        i32TeleZoomListSize         The size of the zoom list.
* @return           MOK if success, otherwise fail.
*/
ARC_DCVOZ_API MRESULT ARC_DCVOZ_SetZoomList(
        MHandle        hHandle,
        MFloat*        pfWideZoomList,
        MInt32         i32WideZoomListSize,
        MFloat*        pfTeleZoomList,
        MInt32         i32TeleZoomListSize
        );

/** @brief Get the recommended zoom value.
*
* @param[in]        hHandle                  Handle of the algorithm engine.
* @param[in]        fViewRatio               The zoom level of user image.
* @param[out]       pfWideImageRatio         The zoom level of wide image.
* @param[out]       pfTeleImageRatio         The zoom level of tele image.
* @param[out]       pbNeedWideFullBuffer        If use full buffer or not.
* @param[out]       pbNeedTeleFullBuffer        If use full buffer or not.
* @return           MOK if success, otherwise fail.
*/
ARC_DCVOZ_API MRESULT ARC_DCVOZ_GetZoomVal(
        MHandle        hHandle,
        MFloat         fViewRatio,
        MFloat*        pfWideImageRatio,
        MFloat*        pfTeleImageRatio,
        MInt32*        pbNeedWideFullBuffer,
        MInt32*        pbNeedTeleFullBuffer
        );


    
/** @brief Perform algorithm.
* @param[in]        hHandle         Handle of the algorithm engine.
* @param[in]        pWideImg        Image of wide image.
* @param[in]        pTeleImg        Image of tele image.
* @param[in]        pRatioParam     The parameter of ratio.
* @param[in]        pCameraParam    The parameter of cameras.
* @param[out]       pDstImg         The result image.
* @param[in]        pFdWideImg      File description of ION for wide image.
* @param[in]        pFdTeleImg      File description of ION for tele image.
* @param[in]        pFdResultImg      File description of ION for result image.
* @return            MOK if success, otherwise fail.
*/
ARC_DCVOZ_API MRESULT ARC_DCVOZ_Process(
        MHandle                    hHandle,
        LPASVLOFFSCREEN            pWideImg,
        LPASVLOFFSCREEN            pTeleImg,
        LPARC_DCVOZ_RATIOPARAM     pRatioParam,
        LPARC_DCVOZ_CAMERAPARAM    pCameraParam,
        LPASVLOFFSCREEN            pDstImg,
        LPARC_DCOZ_IMAGEFD           pFdWideImg,
        LPARC_DCOZ_IMAGEFD           pFdTeleImg,
        LPARC_DCOZ_IMAGEFD           pFdResultImg
        );


/** @brief Get tele rectangle base on result.
* @param[in]        hHandle                     Handle of the algorithm engine.
* @param[in]        pRatioParam                 The parameter of ratio.
* @param[in]        ResultRect               The rectangle in result image that use to display.
* @param[out]       pTeleRect                   Match rectangle in tele image.
* @return            MOK if success, otherwise fail.
*/
ARC_DCVOZ_API MRESULT ARC_DCVOZ_GetTeleRectBaseOnResult(
        MHandle hHandle,
        LPARC_DCVOZ_RATIOPARAM pRatioParam,
        MRECT ResultRect,
        MRECT *pTeleRect
        );

/** @brief Get wide rectangle base on result.
* @param[in]        hHandle                     Handle of the algorithm engine.
* @param[in]        pRatioParam                 The parameter of ratio.
* @param[in]        ResultRect                  The rectangle in result image that use to display.
* @param[out]       pWideRect                   Match rectangle in wide image.
* @return            MOK if success, otherwise fail.
*/
ARC_DCVOZ_API MRESULT ARC_DCVOZ_GetWideRectBaseOnResult(
        MHandle hHandle,
        LPARC_DCVOZ_RATIOPARAM pRatioParam,
        MRECT ResultRect,
        MRECT *pWideRect
        );

/** @brief Get result rectangle base on wide.
* @param[in]        hHandle                     Handle of the algorithm engine.
* @param[in]        pRatioParam                 The parameter of ratio.
* @param[in]        WideRect                    Match rectangle in wide image.
* @param[in]        WideNeedMarginInfo          If need margin or not.
* @param[out]       pResultRect                 The rectangle in result image that use to display.
* @return            MOK if success, otherwise fail.
*/
ARC_DCVOZ_API MRESULT ARC_DCVOZ_GetResultRectBaseOnWide(
        MHandle hHandle,
        LPARC_DCVOZ_RATIOPARAM pRatioParam,
        MRECT WideRect,
        MInt32 WideNeedMarginInfo,
        MRECT *pResultRect
        );

/** @brief Get result rectangle base on tele.
* @param[in]        hHandle                     Handle of the algorithm engine.
* @param[in]        pRatioParam                 The parameter of ratio.
* @param[in]        TeleRect                    Match rectangle in tele image.
* @param[in]        TeleNeedMarginInfo          If need margin or not.
* @param[out]       pResultRect                 The rectangle in result image that use to display.
* @return            MOK if success, otherwise fail.
*/
ARC_DCVOZ_API MRESULT ARC_DCVOZ_GetResultRectBaseOnTele(
        MHandle hHandle,
        LPARC_DCVOZ_RATIOPARAM pRatioParam,
        MRECT TeleRect,
        MInt32 TeleNeedMarginInfo,
        MRECT *pResultRect
        );

/** @brief Get parameter for preview and record sync.
* @param[in]        hHandle                     Handle of the algorithm engine.
* @param[out]        pPassParam                 The parameter for preview and record sync.
* @return            MOK if success, otherwise fail.
*/
ARC_DCVOZ_API  MRESULT ARC_DCVOZ_GetSwitchModePassParam(
        MHandle hHandle,
        LPARC_DCVOZ_RECORDING_PARAM pPassParam
        );

/** @brief Set parameter for preview and record sync.
* @param[in]        hHandle                     Handle of the algorithm engine.
* @param[in]        pPassParam                 The parameter for preview and record sync.
* @return            MOK if success, otherwise fail.
*/
ARC_DCVOZ_API  MRESULT ARC_DCVOZ_SetSwitchModePassParam(
        MHandle hHandle,
        LPARC_DCVOZ_RECORDING_PARAM pPassParam
        );

#ifdef __cplusplus
}
#endif

#endif //__ARCSOFT_DUALCAM_VIDEO_OPTICAL_ZOOM_H__20161003_


