#ifndef __SSDF_DSP_COMMON_INCLUDE_SC_NEIGHBOR_H__
#define __SSDF_DSP_COMMON_INCLUDE_SC_NEIGHBOR_H__

#include <sc_types.h>

/// @brief  Structure to contain neighbor size of tile
/// @ingroup include
///
/// sc_neighbor has been used only in sequencer related
///FIXME is this need to move dma? or something else?
typedef struct _sc_neighbor{
  sc_u32 left         :8; ///< left padding size in the tile
  sc_u32 right        :8; ///< right padding size in the tile
  sc_u32 top          :8; ///< top padding size in the tile
  sc_u32 bottom       :8; ///< bottom padding size in the tile
} sc_neighbor;


#endif
