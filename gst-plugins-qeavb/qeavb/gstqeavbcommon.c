/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * (IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <gst/gstinfo.h>
#include "gstqeavbcommon.h"

int qeavb_create_stream_remote(int eavb_fd, char* file_path, eavb_ioctl_hdr_t* hdr)
{
  GST_INFO ("Calling %s() with par: fd %d, file %s, hdr %p", __func__, eavb_fd, file_path==NULL?"NULL":file_path, hdr);
  return qavb_create_stream_remote(eavb_fd, file_path, hdr);
}

int qeavb_get_stream_info(int eavb_fd, eavb_ioctl_hdr_t* hdr, eavb_ioctl_stream_info_t* info)
{
  GST_INFO ("Calling %s() with par: fd %d, hdr %p, info %p", __func__, eavb_fd, hdr, info);
  return qavb_get_stream_info(eavb_fd, hdr, info);
}

int qeavb_destroy_stream(int eavb_fd, eavb_ioctl_hdr_t* hdr)
{
  GST_INFO ("Calling %s() with par: fd %d, hdr %p", __func__, eavb_fd, hdr);
  return qavb_destroy_stream(eavb_fd, hdr);
}

int qeavb_connect_stream(int eavb_fd, eavb_ioctl_hdr_t* hdr)
{
  GST_INFO ("Calling %s() with par: fd %d, hdr %p", __func__, eavb_fd, hdr);
  return qavb_connect_stream(eavb_fd, hdr);
}

int qeavb_disconnect_stream(int eavb_fd, eavb_ioctl_hdr_t* hdr)
{
  GST_INFO ("Calling %s() with par: fd %d, hdr %p", __func__, eavb_fd, hdr);
  return qavb_disconnect_stream(eavb_fd, hdr);
}

int qeavb_receive_data(int eavb_fd, eavb_ioctl_hdr_t* hdr, eavb_ioctl_buf_data_t* buff)
{
  GST_INFO ("Calling %s() with par: fd %d, hdr %p, buff %p", __func__, eavb_fd, hdr, buff);
  return qavb_receive(eavb_fd, hdr, buff);
}

int qeavb_receive_done(int eavb_fd, eavb_ioctl_hdr_t* hdr, eavb_ioctl_buf_data_t* data)
{
  GST_INFO ("Calling %s() with par: fd %d, hdr %p, buff %p", __func__, eavb_fd, hdr, data);
  return qavb_receive_done(eavb_fd, hdr, data);
}

int kpi_place_marker(const char* str)
{
  int fd = open("/dev/mpm", O_WRONLY);
  if(fd >= 0) {
    int ret = write(fd, str, strlen(str));
    close(fd);
    return ret;
  }
  return -1;
}
