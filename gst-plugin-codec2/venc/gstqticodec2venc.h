/*
* Copyright (c) 2021, The Linux Foundation. All rights reserved.
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
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __GST_QTICODEC2VENC_H__
#define __GST_QTICODEC2VENC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>
#include <gst/video/gstvideopool.h>
#include <gst/allocators/allocators.h>
#include "gstqticodec2bufferpool.h"

#include "codec2wrapper.h"

G_BEGIN_DECLS

#define GST_TYPE_QTICODEC2VENC          (gst_qticodec2venc_get_type())
#define GST_QTICODEC2VENC(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_QTICODEC2VENC,Gstqticodec2venc))
#define GST_QTICODEC2VENC_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_QTICODEC2VENC,Gstqticodec2vencClass))
#define GST_IS_QTICODEC2VENC(obj)       (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_QTICODEC2VENC))
#define GST_IS_QTICODEC2VENC_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_QTICODEC2VENC))

typedef struct _Gstqticodec2venc      Gstqticodec2venc;
typedef struct _Gstqticodec2vencClass Gstqticodec2vencClass;

/* Maximum number of input frame queued */
#define MAX_QUEUED_FRAME  32

struct _Gstqticodec2venc
{
  GstVideoDecoder parent;

  /* Public properties */
  gboolean silent;

  void *comp_store;
  void *comp;
  void *comp_intf;

  guint64 queued_frame[MAX_QUEUED_FRAME];

  GstBufferPool *pool;
  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;

  gboolean input_setup;
  gboolean output_setup;
  gboolean eos_reached;

  gint width;
  gint height;
  GstVideoFormat input_format;
  gchar* streamformat;
  guint64 frame_index;
  guint64 num_input_queued;
  guint64 num_output_done;

  GstVideoInterlaceMode interlace_mode;
  GstVideoFormat outPixelfmt;
  RC_MODE_TYPE rcMode;
  guint32 downscale_width;
  guint32 downscale_height;

  GMutex pending_lock;
  GCond  pending_cond;
};

/*
  Class structure should always contain the class structure for the type you're inheriting from.
*/
struct _Gstqticodec2vencClass
{
  GstVideoEncoderClass parent_class;
};

GType gst_qticodec2venc_get_type (void);

G_END_DECLS

#endif /* __GST_QTICODEC2VENC_H__ */
