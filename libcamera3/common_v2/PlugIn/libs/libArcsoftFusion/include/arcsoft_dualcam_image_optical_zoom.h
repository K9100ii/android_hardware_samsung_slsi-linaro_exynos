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

#ifndef __ARCSOFT_DUALCAM_IMAGE_OPTICAL_ZOOM_H__20161003_
#define __ARCSOFT_DUALCAM_IMAGE_OPTICAL_ZOOM_H__20161003_

#include "amcomdef.h"
#include "asvloffscreen.h"
#include "merror.h"
#include "ammem.h"
#include "arcsoft_dualcam_optical_zoom_common.h"

    
#ifdef DUALCAMIMAGEOPTICALZOOMDLL_EXPORTS
#define ARC_DCIOZ_API __declspec(dllexport)
#else
#define ARC_DCIOZ_API
#endif

#ifdef __cplusplus
extern "C" {
#endif
typedef struct _tagARC_DCIOZ_CAMERAIMAGEINFO
{
    MInt32                i32WideFullWidth;                //[in] Width of wide full image by ISP.
    MInt32                i32WideFullHeight;               //[in] Height of wide full image by ISP.
    MInt32                i32TeleFullWidth;                //[in] Width of tele full image by ISP.
    MInt32                i32TeleFullHeight;               //[in] Height of tele full image by ISP.
}ARC_DCIOZ_CAMERAIMAGEINFO, *LPARC_DCIOZ_CAMERAIMAGEINFO;

typedef struct _tagARC_DCIOZ_RATIOPARAM
{
    MFloat fViewRatio;                            //[in] The zoom level of user image.
    MFloat fWideImageRatio;                       //[in] The zoom level of wide image.
    MFloat fTeleImageRatio;                       //[in] The zoom level of tele image.
}ARC_DCIOZ_RATIOPARAM, *LPARC_DCIOZ_RATIOPARAM;

typedef struct _tagARC_DCIOZ_CAMERAPARAM
{
    MInt32 ISOWide;                   //[in]  ISO of Wide camera.
    MInt32 ISOTele;                   //[in]  ISO of Tele camera.
    ARC_DCOZ_Fraction EVWide;                    // [in] Exposure Value of Wide camera
    ARC_DCOZ_Fraction EVTele;                    // [in] Exposure Value of Tele camera
    ARC_DCOZ_Fraction exposureTimeWide;          // [in] Exposure Time of Wide camera
    ARC_DCOZ_Fraction exposureTimeTele;          // [in] Exposure Time of Tele camera
    MFloat WideObjectDistanceValue;    //[in] Focus position of wide camera
    MFloat TeleObjectDistanceValue;    //[in] Focus position of tele camera
    MInt32 Orientation;                          // [in] Orientation
    MInt32 WideFocusFlag;           //[in]  Wide finish the automatic focus or not.
    MInt32 TeleFocusFlag;           //[in]  Tele finish the automatic focus or not.
    ARC_DCOZ_RECT WideFocusRoi;        //[in] Wide camera focus roi
    ARC_DCOZ_RECT TeleFocusRoi;        //[in] Tele camera focus roi
    ARC_DCOZ_FACEINFO WideFaceInfo;   //[in] Face rectangle info in wide image
    ARC_DCOZ_FACEINFO TeleFaceInfo;   //[in] Face rectangle info in tele image
    ARC_DCOZ_RECT OriginalWideROI;    //[in] Crop ROI in wide 1.0 in accord with preview
}ARC_DCIOZ_CAMERAPARAM, *LPARC_DCIOZ_CAMERAPARAM;

/** @brief Get version information of the algorithm.
* 
* @return    The version information.
*/
ARC_DCIOZ_API const MPBASE_Version *ARC_DCIOZ_GetVersion();


/** @brief Initialize the algorithm engine.
* 
* @param[in/out]    phHandle      Handle of the algorithm engine.
*
* @param[in]        pInitParam    The initial parameter.
* @return       MOK if success, otherwise fail.
*/
ARC_DCIOZ_API MRESULT ARC_DCIOZ_Init(
        MHandle*             phHandle,
        LPARC_DCOZ_INITPARAM pInitParam
        );

/** @brief Uninitialize the algorithm engine.
* 
* @param[in/out]    hHandle        Handle of the algorithm engine.
* @return   MOK if success, otherwise fail.
*/
ARC_DCIOZ_API MRESULT ARC_DCIOZ_Uninit(MHandle* phHandle);

/** @brief Set the zoom list of camera.
* @param[in/out]    hHandle                     Handle of the algorithm engine.
* @param[in]        pi32WideZoomList            The zoom list of wide camera.
* @param[in]        i32WideZoomListSize         The size of the zoom list.
* @param[in]        pi32TeleZoomList            The zoom list of tele camera.
* @param[in]        i32TeleZoomListSize         The size of the zoom list.
* @return           MOK if success, otherwise fail.
*/
ARC_DCIOZ_API MRESULT ARC_DCIOZ_SetZoomList(
        MHandle        hHandle,
        MFloat*        pfWideZoomList,
        MInt32         i32WideZoomListSize,
        MFloat*        pfTeleZoomList,
        MInt32         i32TeleZoomListSize
        );

/** @brief    Get the recommended zoom value.
*
* @param[in/out]    hHandle                    Handle of the algorithm engine.
* @param[in]        fViewRatio                The zoom level of user image.
* @param[out]        pfWideImageRatio        The zoom level of wide image.
* @param[out]        pfTeleImageRatio        The zoom level of tele image.
* @return            MOK if success, otherwise fail.
*/
ARC_DCIOZ_API MRESULT ARC_DCIOZ_GetZoomVal(
        MHandle        hHandle,
        MFloat         fViewRatio,
        MFloat*        pfWideImageRatio,
        MFloat*        pfTeleImageRatio
        );

/** @brief
* @param[in]        hHandle                     Handle of the algorithm engine.
* @param[in]        pCameraImageInfo            Image information.
* @return            MOK if success, otherwise fail.
*/
#define ARC_DCOZ_MODE_C        3  //for Snapshot, Input image with zoom. Output target size.
#define ARC_DCOZ_MODE_E        7  //for Snapshot, Input image with zoom as 1.0f and Output target size. 
ARC_DCIOZ_API MRESULT ARC_DCIOZ_SetImageSize(
        MHandle        hHandle,
        LPARC_DCIOZ_CAMERAIMAGEINFO    pCameraImageInfo,
        MInt32                i32Mode
        );

/** @brief Get crop ROI and ratio. Call it before ARC_DCIOZ_Process.
* @param[in]        hHandle         Handle of the algorithm engine.
* @param[in]        pWideImg        Image of wide image.
* @param[in]        pTeleImg        Image of tele image.
* @param[in]        pRatioParam        The parameter of ratio.
* @param[in]        CameraIndex        From preview, Result image is base on wide or tele camera. The wide is ARC_DCOZ_BASEWIDE. The tele is ARC_DCOZ_BASETELE.
* @param[in]        i32ImageShiftX        From preview, capture image x offset.
* @param[in]        i32ImageShiftY        From preview, capture image y offset.
* @param[in/out]    pWideCropROI    The crop ROI of wide image.
* @param[in/out]    pTeleCropROI    The crop ROI of tele image.
* @param[in/out]    pfWideCropROIRatio    The ratio of wide image's crop ROI.
* @param[in/out]    pfTeleCropROIRatio    The ratio of tele image's crop ROI.
* @return            MOK if success, otherwise fail.
*/
ARC_DCIOZ_API MRESULT ARC_DCIOZ_GetInputCropROI(
        MHandle                    hHandle,
        LPASVLOFFSCREEN            pWideImg,
        LPASVLOFFSCREEN            pTeleImg,
        LPARC_DCIOZ_RATIOPARAM     pRatioParam,
        MInt32                     CameraIndex,
        MInt32                     i32ImageShiftX,
        MInt32                     i32ImageShiftY,
        MRECT*                     pWideCropROI,
        MRECT*                     pTeleCropROI,
        MFloat*                    pfWideCropROIRatio,
        MFloat*                    pfTeleCropROIRatio
    );

/** @brief 
* @param[in/out]    hHandle         Handle of the algorithm engine.
* @param[in]        pWideImg        Image of wide image.
* @param[in]        pTeleImg        Image of tele image.
* @param[in]        pRatioParam     The parameter of ratio.
* @param[in/out]    pCameraParam    The parameter of cameras.
* @param[out]        pDstImg        The result image.
* @param[in]        pFdWideImg      File description of ION for wide image.
* @param[in]        pFdTeleImg      File description of ION for tele image.
* @param[in]        pFdResultImg      File description of ION for result image.
* @return            MOK if success, otherwise fail.
*/
ARC_DCIOZ_API MRESULT ARC_DCIOZ_Process(
        MHandle                    hHandle,
        LPASVLOFFSCREEN            pWideImg,
        LPASVLOFFSCREEN            pTeleImg,
        LPARC_DCIOZ_RATIOPARAM     pRatioParam,
        LPARC_DCIOZ_CAMERAPARAM    pCameraParam,
        LPASVLOFFSCREEN            pDstImg,
        LPARC_DCOZ_IMAGEFD           pFdWideImg,
        LPARC_DCOZ_IMAGEFD           pFdTeleImg,
        LPARC_DCOZ_IMAGEFD           pFdResultImg
        );


/** @brief
* @param[in]    hHandle                       Handle of the algorithm engine.
* @param[out]    pMetaData                    Pointer of Metadata
* @param[out]    pMetaDataSize                Size of Metadata
* @return        MOK if success, otherwise fail.
*/
ARC_DCIOZ_API  MRESULT ARC_DCIOZ_GetMetaData(
        MHandle                hHandle,
        MVoid**                ppMetaData,
        MInt32*                pMetaDataSize
    );


#ifdef __cplusplus
}
#endif

#endif //__ARCSOFT_DUALCAM_IMAGE_OPTICAL_ZOOM_H__20161003_


