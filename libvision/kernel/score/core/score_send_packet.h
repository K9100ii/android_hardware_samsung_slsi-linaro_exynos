//------------------------------------------------------------------------------
/// @file  send_packet.h
/// @ingroup  exynos8895
///
/// @brief  Declarations of ExynosSendPacket class
/// @author  Dongseok Han<ds0920.han@samsung.com>
///
/// @section changelog Change Log
/// 2016/07/18 Dongseok Han created
///
/// @section copyright_section Copyright
/// &copy; 2016, Samsung Electronics Co., Ltd.
///
/// @section license_section License
//------------------------------------------------------------------------------
#ifndef COMMON_EXYNOS8895_SEND_PACKET_H_
#define COMMON_EXYNOS8895_SEND_PACKET_H_

#include "score_send_packet_delegate.h"

#include "vs4l.h"
#include "vs4l_buffer_helper.h"

namespace score {

/// @brief  Class for SendPacket class depends on the platform
class ExynosSendPacket : public SendPacketDelegate {
  public:
    /// @brief  Constructor of ExynosSendPacket class
    ///
    /// Initializing static values of structures
    ExynosSendPacket();
    /// @brief  Destructor of ExynosSendPacket class
    ///
    /// This is empty
    ~ExynosSendPacket() {}

    /// @brief  Implementations of SendPacket interface
    /// @param  fd  File desciptor of SCoreDevice
    /// @param  packet  packet class be sent to SCore
    /// @retval 0 if success, otherwise non-zero
    virtual int SendPacket(int fd, const ScoreIpcPacket *const packet);
    virtual int SetControl(int fd, unsigned int cmd, unsigned int val);

  private:
    /// @brief  Update packet size of private member variables
    /// @param  packet_size   packet size to update
    void UpdatePacketSize(size_t packet_size);
    /// @brief  Update packet pointer of private member variables
    /// @param  packet_addr   address of packet
    void UpdatePacketBuffer(unsigned long packet_addr);

  private:
    /// @{
    /// List of the structures which are used to communicate
    /// with score on the Exynos8895
    struct vs4l_request_config  user_config;
    struct vs4l_format          in_format;
    struct vs4l_format          ot_format;
    struct vs4l_container_list  in_container_list;
    struct vs4l_container_list  ot_container_list;
    struct vs4l_container       in_container;
    struct vs4l_container       ot_container;
    struct vs4l_buffer          in_buffer;
    struct vs4l_buffer          ot_buffer;
    /// @}
};

} //namespace score
#endif
