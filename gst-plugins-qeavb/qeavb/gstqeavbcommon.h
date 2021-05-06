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

#include  <stdlib.h>
#include  <string.h>
#include <stdio.h>
#include <time.h>
#include <sched.h>
//#include <math.h>
#include <pthread.h>
#include <sys/stat.h>

#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <gst/gst.h>

#ifndef false
typedef enum { false = 0, true = 1 } bool;
#endif

#if /* Supported Compilers */ \
    defined(__ARMCC_VERSION) || \
    defined(__GNUC__)

  /* If we're hosted, fall back to the system's stdint.h, which might have
   * dditional definitions.
   */

#include "stdint.h"
#else /* Unsupported Compilers */

  /* The following definitions are the same accross platforms.  This first
   * group are the sanctioned types.
   */

  typedef unsigned long long uint64_t;  /* Unsigned 64 bit value */
  typedef unsigned long int  uint32_t;  /* Unsigned 32 bit value */
  typedef unsigned short     uint16_t;  /* Unsigned 16 bit value */
  typedef unsigned char      uint8_t;   /* Unsigned 8  bit value */

  typedef signed long long   int64_t;   /* Signed 64 bit value */
  typedef signed long int    int32_t;   /* Signed 32 bit value */
  typedef signed short       int16_t;   /* Signed 16 bit value */
  typedef signed char        int8_t;    /* Signed 8  bit value */

#endif /* Supported Compilers */

#include "eavb_shared.h"

#define QEAVB_PCM_DEFAULT_BLOCKSIZE 1500
#define QEAVB_TS_DEFAULT_BLOCKSIZE 1500
#define DEFALUT_SLEEP_US 10000
#define RETRY_COUNT 100000
#define MAC_STR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_TO_STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]

#define STREAMID_STR "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"
#define STREAMID_NET_TO_STR(a) (a)[7], (a)[6], (a)[5], (a)[4], (a)[3], (a)[2], (a)[1], (a)[0]
#define STREAMID_TO_STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7]

#define DEF_MAX_STALE_NS 5000000
#define MAX_STALE_MS 10000 //10 sec

#define PRIi64       "I64u"
#define PRIu64       "I64u"
#define PRIx64       "I64x"

enum {
    QEAVB_CRF_MODE_DISABLED = 0,    // CRF disabled
    QEAVB_CRF_MODE_TALKER,          // CRF talker
    QEAVB_CRF_MODE_LISTENER,        // CRF listener, PPM = local - remote
    QEAVB_CRF_MODE_LISTENER_NOMINAL,// CRF listener, PPM = nominal - remote
    QEAVB_CRF_MODE_MAX
} qeavb_crf_mode;

enum {
    QEAVB_CVF_FORMAT_RESERVED = 0,
    QEAVB_CVF_FORMAT_RFC,
} qeavb_cvf_format;

enum {
    QEAVB_CVF_FORMAT_SUBTYPE_MJPEG = 0,
    QEAVB_CVF_FORMAT_SUBTYPE_H264,
    QEAVB_CVF_FORMAT_SUBTYPE_JPEG2000
} qeavb_cvf_format_subtype;

enum {
    QEAVB_CRF_TYPE_USER = 0,        // user specified
    QEAVB_CRF_TYPE_AUDIO_SAMPLE,    // audio ample timestamp
    QEAVB_CRF_TYPE_VIDEO_FRAME,     // video frame sync timestamp
    QEAVB_CRF_TYPE_VIDEO_LINE,      // video line timestamp
    QEAVB_CRF_TYPE_MACHINE_CYCLE,   // machine cycle timestamp
    QEAVB_CRF_TYPE_MAX
} qeavb_crf_type;

enum {
    QEAVB_CRF_PULL_1_DIV_1_0 = 0,       // Multiply base_frequency field by 1.0
    QEAVB_CRF_PULL_1_DIV_1_DOT_1001,    // Multiply base_frequency field by 1/1.1001
    QEAVB_CRF_PULL_1_DOT_1001,          // Multiply base_frequency field by 1.1001
    QEAVB_CRF_PULL_24_DIV_25,           // Multiply base_frequency field by 24/25
    QEAVB_CRF_PULL_25_DIV_24,           // Multiply base_frequency field by 25/24
    QEAVB_CRF_PULL_8,                    // Multiply base_frequency field by 8
    QEAVB_CRF_PULL_MAX
} qeavb_crf_pull;

int qeavb_read_config_file(eavb_ioctl_stream_config_t* streamCtx, char* filepath);
int qeavb_create_stream(int eavb_fd, eavb_ioctl_stream_config_t* config, eavb_ioctl_hdr_t* hdr);
int qeavb_create_stream_remote(int fd, char* cfgfilepath, eavb_ioctl_hdr_t* hdr);
int qeavb_get_stream_info(int eavb_fd, eavb_ioctl_hdr_t* hdr, eavb_ioctl_stream_info_t* info);
int qeavb_destroy_stream(int eavb_fd, eavb_ioctl_hdr_t* hdr);
int qeavb_connect_stream(int eavb_fd, eavb_ioctl_hdr_t* hdr);
int qeavb_disconnect_stream(int eavb_fd, eavb_ioctl_hdr_t* hdr);
int qeavb_receive_data(int eavb_fd, eavb_ioctl_hdr_t* hdr, eavb_ioctl_buf_data_t* buff);
int qeavb_receive_done(int eavb_fd, eavb_ioctl_hdr_t* hdr, eavb_ioctl_buf_data_t* data);
int kpi_place_marker(const char* str);

#endif /* __GST_QEAVB_COMMON_H__ */

