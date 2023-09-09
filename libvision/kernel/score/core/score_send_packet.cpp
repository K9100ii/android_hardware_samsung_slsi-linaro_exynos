//------------------------------------------------------------------------------
/// @file  send_packet.cc
/// @ingroup  exynos8895
///
/// @brief  Implementations of ExynosSendPacket class
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
#include <stdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#include "score_send_packet.h"
#include "score_device_real.h"
#include "score_ioctl.h"
#include "logging.h"
#include "sc_data_type.h"


namespace score {

#define BUFFER_CNT    1

ExynosSendPacket::ExynosSendPacket() {
  memset(&user_config, 0, sizeof(user_config));
  user_config.buffer_cnt = BUFFER_CNT;

  // in flist and format
  user_config.incfg.flist.direction = VS4L_DIRECTION_IN;
  user_config.incfg.flist.count     = 1;
  user_config.incfg.flist.formats   = &in_format;

  in_format.target  = 0;
  in_format.height  = 1;
  in_format.plane   = 0;
  in_format.format  = VS4L_DF_IMAGE_U8;

  // out flist and format
  user_config.otcfg.flist.direction = VS4L_DIRECTION_OT;
  user_config.otcfg.flist.count     = 1;
  user_config.otcfg.flist.formats   = &ot_format;

  ot_format.target  = 0;
  ot_format.height  = 1;
  ot_format.plane   = 0;
  ot_format.format  = VS4L_DF_IMAGE_U8;

  // in clist & container & buffer
  user_config.incfg.clist       = &in_container_list;

  in_container_list.direction   = VS4L_DIRECTION_IN;
  in_container_list.id          = 0;
  in_container_list.index       = 0;
  in_container_list.flags       = 0;
  in_container_list.count       = 1;
  in_container_list.containers  = &in_container;

  in_container.type             = VS4L_BUFFER_LIST;
  in_container.target           = 0;
  in_container.count            = 1;
  in_container.memory           = VS4L_MEMORY_VIRTPTR;
  in_container.buffers          = &in_buffer;

  in_buffer.roi.x               = 0;
  in_buffer.roi.y               = 0;
  in_buffer.roi.h               = 1;
  in_buffer.reserved            = 0;

  user_config.otcfg.clist       = &ot_container_list;

  ot_container_list.direction   = VS4L_DIRECTION_OT;
  ot_container_list.id          = 0;
  ot_container_list.index       = 0;
  ot_container_list.flags       = 0;
  ot_container_list.count       = 1;
  ot_container_list.containers  = &ot_container;

  ot_container.type             = VS4L_BUFFER_LIST;
  ot_container.target           = 0;
  ot_container.count            = 1;
  ot_container.memory           = VS4L_MEMORY_VIRTPTR;
  ot_container.buffers          = &ot_buffer;

  ot_buffer.roi.x               = 0;
  ot_buffer.roi.y               = 0;
  ot_buffer.roi.h               = 1;
  ot_buffer.reserved            = 0;

}

void ExynosSendPacket::UpdatePacketSize(size_t packet_size) {
  in_format.width = in_buffer.roi.w = packet_size;
  ot_format.width = ot_buffer.roi.w = packet_size;
}

void ExynosSendPacket::UpdatePacketBuffer(unsigned long packet_addr) {
  in_buffer.m.userptr = ot_buffer.m.userptr = packet_addr;
}

int ExynosSendPacket::SendPacket(int fd, const ScoreIpcPacket *const packet) {
  int ret;

  //struct vs4l_request_config *user_config = NULL;
  size_t packet_size = sizeof(ScoreIpcPacket) + \
    (sizeof(ScorePacketGroup) * packet->size.group_count);
  UpdatePacketSize(packet_size);
  UpdatePacketBuffer((unsigned long)packet);

  ret = ioctl(fd, VS4L_VERTEXIOC_S_FORMAT, &user_config.incfg.flist);
  if (ret)
    sc_error("%s():%d\n", __func__, ret);
  ret = ioctl(fd, VS4L_VERTEXIOC_S_FORMAT, &user_config.otcfg.flist);
  if (ret)
    sc_error("%s():%d\n", __func__, ret);
  ret = ioctl(fd, VS4L_VERTEXIOC_STREAM_ON);
  if (ret)
    sc_error("%s():%d\n", __func__, ret);
  ret = ioctl(fd, VS4L_VERTEXIOC_QBUF, user_config.incfg.clist);
  if (ret)
    sc_error("%s():%d\n", __func__, ret);
  ret = ioctl(fd, VS4L_VERTEXIOC_QBUF, user_config.otcfg.clist);
  if (ret)
    sc_error("%s():%d\n", __func__, ret);

  ret = ioctl(fd, VS4L_VERTEXIOC_DQBUF, user_config.incfg.clist);
  if (ret)
    sc_error("%s():%d\n", __func__, ret);
  ret = ioctl(fd, VS4L_VERTEXIOC_DQBUF, user_config.otcfg.clist);
  if (ret)
    sc_error("%s():%d\n", __func__, ret);
  ret = ioctl(fd, VS4L_VERTEXIOC_STREAM_OFF);
  if (ret)
    sc_error("%s():%d\n", __func__, ret);

  return 0;
}

int ExynosSendPacket::SetControl(int fd, unsigned int cmd, unsigned int val) {
  int ret = 0;
  struct vs4l_ctrl ctrl;

  sc_error("[%s(%d)] score dev fd(%d) \n", __func__,__LINE__, fd);

  ctrl.ctrl = cmd;
  ctrl.value = val;

  ret = ioctl(fd, VS4L_VERTEXIOC_S_CTRL, &ctrl);
  if (ret)
    sc_error("%s():%d\n", __func__, ret);

  return 0;
}

} //namespace score
