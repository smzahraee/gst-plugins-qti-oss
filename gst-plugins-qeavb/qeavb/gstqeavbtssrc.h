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

#ifndef __GST_QEAVB_TS_SRC_H__
#define __GST_QEAVB_TS_SRC_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/audio/audio.h>

#include "gstqeavbcommon.h"

G_BEGIN_DECLS

#define GST_TYPE_QEAVB_TS_SRC (gst_qeavb_ts_src_get_type())
#define GST_QEAVB_TS_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_QEAVB_TS_SRC,GstQeavbTsSrc))
#define GST_QEAVB_TS_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_QEAVB_TS_SRC,GstQeavbTsSrcClass))
#define GST_IS_QEAVB_TS_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_QEAVB_TS_SRC))
#define GST_IS_QEAVB_TS_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_QEAVB_TS_SRC))

typedef struct _GstQeavbTsSrc GstQeavbTsSrc;
typedef struct _GstQeavbTsSrcClass GstQeavbTsSrcClass;

struct _GstQeavbTsSrc
{
  GstPushSrc parent;

  void* eavb_addr;
  gchar * config_file;

  int eavb_fd;
  eavb_ioctl_hdr_t hdr;
  eavb_ioctl_stream_info_t stream_info;
  eavb_ioctl_stream_config_t cfg_data;
  gboolean started;
  gboolean is_first_tspacket;
  GMutex lock;
};

struct _GstQeavbTsSrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_qeavb_ts_src_get_type (void);

gboolean gst_qeavb_ts_src_plugin_init (GstPlugin * plugin);


G_END_DECLS

#endif /* __GST_QEAVB_TS_SRC_H__ */
