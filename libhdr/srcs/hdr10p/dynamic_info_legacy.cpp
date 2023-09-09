#include "dynamic_info_legacy.h"

#ifndef USE_FULL_ST2094_40
void convertDynamicMeta(ExynosHdrDynamicInfo_t *dyn_meta, ExynosHdrDynamicInfo *dynamic_metadata)
{
    return;
}
#else
void convertDynamicMeta(ExynosHdrDynamicInfo_t *dyn_meta, ExynosHdrDynamicInfo *dynamic_metadata)
{
    int i;
    dyn_meta->valid                                     = dynamic_metadata->valid;
    dyn_meta->data.country_code                         = dynamic_metadata->data.country_code;
    dyn_meta->data.provider_code                        = dynamic_metadata->data.provider_code;
    dyn_meta->data.provider_oriented_code               = dynamic_metadata->data.provider_oriented_code;
    dyn_meta->data.application_identifier               = dynamic_metadata->data.application_identifier;
    dyn_meta->data.application_version                  = dynamic_metadata->data.application_version;
    dyn_meta->data.display_maximum_luminance            = dynamic_metadata->data.targeted_system_display_maximum_luminance;
    for (i = 0; i < 3; i++)
        dyn_meta->data.maxscl[i]                        = dynamic_metadata->data.maxscl[0][i];
    dyn_meta->data.num_maxrgb_percentiles               = dynamic_metadata->data.num_maxrgb_percentiles[0];
    for (i = 0; i < 15; i++) {
        dyn_meta->data.maxrgb_percentages[i]            = dynamic_metadata->data.maxrgb_percentages[0][i];
        dyn_meta->data.maxrgb_percentiles[i]            = dynamic_metadata->data.maxrgb_percentiles[0][i];
    }
    dyn_meta->data.tone_mapping.tone_mapping_flag       = dynamic_metadata->data.tone_mapping.tone_mapping_flag[0];
    dyn_meta->data.tone_mapping.knee_point_x            = dynamic_metadata->data.tone_mapping.knee_point_x[0];
    dyn_meta->data.tone_mapping.knee_point_y            = dynamic_metadata->data.tone_mapping.knee_point_y[0];
    dyn_meta->data.tone_mapping.num_bezier_curve_anchors= dynamic_metadata->data.tone_mapping.num_bezier_curve_anchors[0];
    for (i = 0; i < 15; i++)
        dyn_meta->data.tone_mapping.bezier_curve_anchors[i] = dynamic_metadata->data.tone_mapping.bezier_curve_anchors[0][i];
}
#endif
