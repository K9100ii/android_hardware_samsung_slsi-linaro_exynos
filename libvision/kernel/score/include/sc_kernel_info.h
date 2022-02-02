#ifndef SCORE_SC_KERNEL_INFO_H_
#define SCORE_SC_KERNEL_INFO_H_

#include <sc_neighbor.h>
#include <sc_types.h>
#include <sc_buf_type.h>
#include <sc_kernel_policy.h>
#include <score_ap1_dma_mode.h>

#define MAX_PARAM 10

/// @brief  Type of each parameter to a SCore kernel
typedef enum
{
  TILE      = 0x0,                    ///< Parameter that requires tiling and DMA
  NONTILE   = 0x1                     ///< Parameter that does not require tiling and DMA
} param_kind_e;

/// @brief  Function pointer type to DMA data conversion mode function
typedef sc_u32 (*sc_cmode_t)(data_buf_type dst_type, data_buf_type src_type);

/// @brief  Kernel parameter descriptor
///
/// sc_param_t is defiend for being used inside kernel information related data structure.
typedef struct _sc_param_desc_t {
  union {
    /// Header information
    struct {
      data_buf_type type;             ///< Parameter type such as TYPE_U8, TYPE_RGB
      param_kind_e is_tile: 1;        ///< TILE or NONTILE
    } header;

    /// Tile information
    struct {
      data_buf_type type;             ///< Parameter type such as TYPE_U8, TYPE_RGB
      param_kind_e is_tile: 1;        ///< TILE or NONTILE
      unsigned direction: 1;          ///< Direction of DMA transfer (DRMA_TO_TCM or TCM_TO_DRAM)
      unsigned tw: 16;                ///< Tile width recommendation in pixel
      unsigned th: 16;                ///< Tile height recommendation in pixel
      sc_cmode_t cmode;               ///< DMA data conversion mode function
      unsigned tmode;                 ///< DMA data transfer mode
      sc_neighbor neighbor;           ///< Neighbor pixel
    } tile;
  };
} sc_param_desc_t;


/// @brief  Kernel information holder
///
/// This data structure is used for describing any SCore kernel.
/// Here are two examples to use this data sturcture.
///
/// Example of sc_kernel_info_t setting
/// Example 1: Gaussain3x3
/// sc_status_t scv_gaussian3x3(sc_buffer *in, sc_buffer *out);
///
/// sc_kernel_info_t Gaussian3x3Info = {
///   SCV_GAUSSIAN_3X3,
///   NORMAL,
///   2,
///   NULL,
///   { /// Entry sc_param_desc_t array
///     { /// describing 'in' (pinfo[0])
///       TILE( /// tile variable
///           TYPE_U16,
///           DRAM_TO_TCM,
///           32,
///           64,
///           DYNAMIC_DC_NORMAL,
///           DT_NORMAL,
///           neighbor(1,1,1,1)
///           )
///     },
///     { /// describing 'out' (pinfo[1])
///       TILE( /// tile variable
///           TYPE_U16,
///           TCM_TO_DRAM,
///           32,
///           64,
///           DYNAMIC_DC_NORMAL,
///           DT_NORMAL,
///           neighbor(1,1,1,1)
///           )
///     }
///   }
/// };
///
///
///
/// Example 2: Threshold binary
/// sc_status_t scv_threshold_binary(sc_buffer *in, sc_buffer *out, sc_u8 th);
///
/// sc_kernel_info_t ThresholdBinaryInfo = {
///   SCV_THRESHOLD_BINARY,
///   NORMAL,
///   3,
///   NULL,
///   { /// Entry sc_param_desc_t array
///     { /// describing 'in' (pinfo[0])
///       TILE( /// tile variable
///           TYPE_U8,
///           DRAM_TO_TCM,
///           64,
///           64,
///           DYNAMIC_DC_NORMAL,
///           DT_NORMAL,
///           neighbor(0,0,0,0)
///           )
///     },
///     { /// describing 'out' (pinfo[1])
///       TILE( /// tile variable
///           TYPE_U8,
///           TCM_TO_DRAM,
///           64,
///           64,
///           DYNAMIC_DC_NORMAL,
///           DT_NORMAL,
///           neighbor(0,0,0,0)
///           )
///     },
///     { /// describing 'th' (pinfo[2])
///       NONTILE( /// scalar variable
///           TYPE_U8
///           )
///     }
///   }
/// };
///
typedef struct _sc_kernel_info_t
{
  char     kernel_name[40];           ///< SCore kernel name
  unsigned uid:16;                    ///< SCore kernel identifier
  unsigned kernel_type:8;             ///< NORMAL or SPECIAL like copy kernel
  unsigned pcnt:8;                    ///< Number of parameters in a SCore kernel
  union {
    unsigned kernel_addr;
    unsigned long long dummy;
  };                                  ///< SCore target function
  sc_param_desc_t pinfo[MAX_PARAM];   ///< Parameter information of kernel
} sc_kernel_info_t;


#define NONTILE(type) { {type, NONTILE} }
#define TILE(type, direction, tile_width, tile_height, \
    cmode, tmode, neighbor)  \
{ tile:{ type, TILE, direction, tile_width, tile_height, \
    cmode, tmode, neighbor }}


#endif
