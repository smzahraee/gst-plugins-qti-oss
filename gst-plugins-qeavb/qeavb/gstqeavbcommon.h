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

#ifndef __GST_QEAVB_COMMON_H__
#define __GST_QEAVB_COMMON_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <fcntl.h>
#include <stdint.h>
#include<sys/mman.h>
#include "qavblib.h"

#define QEAVB_PCM_DEFAULT_BLOCKSIZE 1500
#define QEAVB_TS_DEFAULT_BLOCKSIZE 1500
#define DEFALUT_SLEEP_US 10000
#define RETRY_COUNT 100000

int qeavb_create_stream_remote(int eavb_fd, char* file_path, eavb_ioctl_hdr_t* hdr);
int qeavb_get_stream_info(int eavb_fd, eavb_ioctl_hdr_t* hdr, eavb_ioctl_stream_info_t* info);
int qeavb_destroy_stream(int eavb_fd, eavb_ioctl_hdr_t* hdr);
int qeavb_connect_stream(int eavb_fd, eavb_ioctl_hdr_t* hdr);
int qeavb_disconnect_stream(int eavb_fd, eavb_ioctl_hdr_t* hdr);
int qeavb_receive_data(int eavb_fd, eavb_ioctl_hdr_t* hdr, eavb_ioctl_buf_data_t* buff);
int qeavb_receive_done(int eavb_fd, eavb_ioctl_hdr_t* hdr, eavb_ioctl_buf_data_t* data);
int kpi_place_marker(const char* str);

#endif /* __GST_QEAVB_COMMON_H__ */

