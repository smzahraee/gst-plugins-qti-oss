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

#ifndef __GST_QTICODEC2VDEC_H__
#define __GST_QTICODEC2VDEC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideodecoder.h>
#include <gst/video/gstvideopool.h>
#include <gst/allocators/allocators.h>

G_BEGIN_DECLS

#define QTICODEC2VDEC_SINK_WH_CAPS    \
  "width  = (int) [ 32, 8192 ], "     \
  "height = (int) [ 32, 8192 ]"

#define QTICODEC2VDEC_SINK_COMPRESSION_CAPS    \
    "compression = (string) { ubwc, linear }"

#define QTICODEC2VDEC_SINK_FPS_CAPS    \
  "framerate = (fraction) [ 0, 480 ]"

#define QTICODEC2VDEC_RAW_CAPS(formats) \
  "video/x-raw, "                       \
  "format = (string) " formats ", "     \
  QTICODEC2VDEC_SINK_WH_CAPS ", "       \
  QTICODEC2VDEC_SINK_FPS_CAPS ", "      \
  QTICODEC2VDEC_SINK_COMPRESSION_CAPS

#define QTICODEC2VDEC_RAW_CAPS_WITH_FEATURES(features, formats) \
  "video/x-raw(" features "), "                                 \
  "format = (string) " formats ", "                             \
  QTICODEC2VDEC_SINK_WH_CAPS   ", "                             \
  QTICODEC2VDEC_SINK_FPS_CAPS  ", "                             \
  QTICODEC2VDEC_SINK_COMPRESSION_CAPS

#define GST_TYPE_QTICODEC2VDEC          (gst_qticodec2vdec_get_type())
#define GST_QTICODEC2VDEC(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_QTICODEC2VDEC,Gstqticodec2vdec))
#define GST_QTICODEC2VDEC_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_QTICODEC2VDEC,Gstqticodec2vdecClass))
#define GST_IS_QTICODEC2VDEC(obj)       (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_QTICODEC2VDEC))
#define GST_IS_QTICODEC2VDEC_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_QTICODEC2VDEC))

typedef struct _Gstqticodec2vdec      Gstqticodec2vdec;
typedef struct _Gstqticodec2vdecClass Gstqticodec2vdecClass;

/* Maximum number of input frame queued */
#define MAX_QUEUED_FRAME  64

struct _Gstqticodec2vdec
{
  GstVideoDecoder parent;

  /* Public properties */
  gboolean silent;

  void *comp_store;
  void *comp;
  void *comp_intf;

  guint64 queued_frame[MAX_QUEUED_FRAME];
  gboolean downstream_supports_gbm;

  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;

  gboolean eos_reached;
  gboolean input_setup;
  gboolean output_setup;

  gint width;
  gint height;
  gchar* streamformat;
  guint64 frame_index;
  GstVideoInterlaceMode interlace_mode;
  GstVideoFormat outPixelfmt;
  guint64 num_input_queued;
  guint64 num_output_done;
  gboolean downstream_supports_dma;
  gboolean output_picture_order_mode;
  gboolean low_latency_mode;
  gboolean map_outbuf;

  GMutex pending_lock;
  GCond  pending_cond;
  struct timeval start_time;
  struct timeval first_frame_time;
  GstBufferPool *out_port_pool;
  void* gbm_lib;
  guint64 (*gbm_api_bo_get_modifier)(void* bo);
  gboolean is_ubwc;
};

/*
  Class structure should always contain the class structure for the type you're inheriting from.
*/
struct _Gstqticodec2vdecClass
{
  GstVideoDecoderClass parent_class;
};

GType gst_qticodec2vdec_get_type (void);

G_END_DECLS

#endif /* __GST_QTICODEC2VDEC_H__ */
