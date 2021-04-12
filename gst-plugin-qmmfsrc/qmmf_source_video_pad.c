/*
* Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
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

#include "qmmf_source_video_pad.h"

#include <gst/gstplugin.h>
#include <gst/gstelementfactory.h>
#include <gst/gstpadtemplate.h>
#include <gst/allocators/allocators.h>

#include "qmmf_source_utils.h"

// Declare qmmfsrc_video_pad_class_init() and qmmfsrc_video_pad_init()
// functions, implement qmmfsrc_video_pad_get_type() function and set
// qmmfsrc_video_pad_parent_class variable.
G_DEFINE_TYPE(GstQmmfSrcVideoPad, qmmfsrc_video_pad, GST_TYPE_PAD);

GST_DEBUG_CATEGORY_STATIC (qmmfsrc_video_pad_debug);
#define GST_CAT_DEFAULT qmmfsrc_video_pad_debug

#define DEFAULT_VIDEO_STREAM_WIDTH    640
#define DEFAULT_VIDEO_STREAM_HEIGHT   480
#define DEFAULT_VIDEO_STREAM_FPS_NUM  30
#define DEFAULT_VIDEO_STREAM_FPS_DEN  1
#define DEFAULT_VIDEO_H264_PROFILE    "high"
#define DEFAULT_VIDEO_H265_PROFILE    "main"
#define DEFAULT_VIDEO_H264_LEVEL      "5.1"
#define DEFAULT_VIDEO_H265_LEVEL      "5.1"
#define DEFAULT_VIDEO_RAW_FORMAT      "NV12"
#define DEFAULT_VIDEO_RAW_COMPRESSION "none"
#define DEFAULT_VIDEO_BAYER_FORMAT    "bggr"
#define DEFAULT_VIDEO_BAYER_BPP       "10"

#define DEFAULT_PROP_SOURCE_INDEX    (-1)
#define DEFAULT_PROP_FRAMERATE       30.0
#define DEFAULT_PROP_BITRATE         6000000
#define DEFAULT_PROP_BITRATE_CONTROL GST_VIDEO_CONTROL_RATE_MAXBITRATE
#define DEFAULT_PROP_QUANT_I_FRAMES  27
#define DEFAULT_PROP_QUANT_P_FRAMES  28
#define DEFAULT_PROP_QUANT_B_FRAMES  28
#define DEFAULT_PROP_MIN_QP          10
#define DEFAULT_PROP_MAX_QP          51
#define DEFAULT_PROP_MIN_QP_I_FRAMES 10
#define DEFAULT_PROP_MAX_QP_I_FRAMES 51
#define DEFAULT_PROP_MIN_QP_P_FRAMES 10
#define DEFAULT_PROP_MAX_QP_P_FRAMES 51
#define DEFAULT_PROP_MIN_QP_B_FRAMES 10
#define DEFAULT_PROP_MAX_QP_B_FRAMES 51
#define DEFAULT_PROP_IDR_INTERVAL    1
#define DEFAULT_PROP_CROP_X          0
#define DEFAULT_PROP_CROP_Y          0
#define DEFAULT_PROP_CROP_WIDTH      0
#define DEFAULT_PROP_CROP_HEIGHT     0

GType
gst_video_pad_control_rate_get_type (void)
{
  static GType gtype = 0;
  static const GEnumValue variants[] = {
    { GST_VIDEO_CONTROL_RATE_DISABLE,
        "Disable", "disable"
    },
    { GST_VIDEO_CONTROL_RATE_VARIABLE,
        "Variable", "variable"
    },
    { GST_VIDEO_CONTROL_RATE_CONSTANT,
        "Constant", "constant"
    },
    { GST_VIDEO_CONTROL_RATE_MAXBITRATE,
        "Max Bitrate", "maxbitrate"
    },
    { GST_VIDEO_CONTROL_RATE_VARIABLE_SKIP_FRAMES,
        "Variable Skip Frames", "constant-skip-frames"
    },
    { GST_VIDEO_CONTROL_RATE_CONSTANT_SKIP_FRAMES,
        "Constant Skip Frames", "variable-skip-frames"
    },
    { GST_VIDEO_CONTROL_RATE_MAXBITRATE_SKIP_FRAMES,
        "Max Bitrate Skip Frames", "maxbitrate-skip-frames"
    },
    {0, NULL, NULL}
  };

  if (!gtype)
    gtype = g_enum_register_static ("GstVideoControlRate", variants);

  return gtype;
}

enum
{
  PROP_0,
  PROP_VIDEO_SOURCE_INDEX,
  PROP_VIDEO_FRAMERATE,
  PROP_VIDEO_BITRATE,
  PROP_VIDEO_BITRATE_CONTROL,
  PROP_VIDEO_QUANT_I_FRAMES,
  PROP_VIDEO_QUANT_P_FRAMES,
  PROP_VIDEO_QUANT_B_FRAMES,
  PROP_VIDEO_MIN_QP,
  PROP_VIDEO_MAX_QP,
  PROP_VIDEO_MIN_QP_I_FRAMES,
  PROP_VIDEO_MAX_QP_I_FRAMES,
  PROP_VIDEO_MIN_QP_P_FRAMES,
  PROP_VIDEO_MAX_QP_P_FRAMES,
  PROP_VIDEO_MIN_QP_B_FRAMES,
  PROP_VIDEO_MAX_QP_B_FRAMES,
  PROP_VIDEO_IDR_INTERVAL,
  PROP_VIDEO_CROP,
};

static void
video_pad_worker_task (GstPad * pad)
{
  GstDataQueue *buffers;
  GstDataQueueItem *item;

  buffers = GST_QMMFSRC_VIDEO_PAD (pad)->buffers;

  if (gst_data_queue_pop (buffers, &item)) {
    GstBuffer *buffer = gst_buffer_ref (GST_BUFFER (item->object));
    item->destroy (item);

    gst_pad_push (pad, buffer);
  } else {
    GST_INFO_OBJECT (pad, "Pause video pad worker thread");
    gst_pad_pause_task (pad);
  }
}

static gboolean
video_pad_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean success = TRUE;

  GST_DEBUG_OBJECT (pad, "Received QUERY %s", GST_QUERY_TYPE_NAME (query));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:
    {
      GstClockTime min_latency, max_latency;

      // Minimum latency is the time to capture one video frame.
      min_latency = GST_QMMFSRC_VIDEO_PAD (pad)->duration;

      // TODO This will change once GstBufferPool is implemented.
      max_latency = GST_CLOCK_TIME_NONE;

      GST_DEBUG_OBJECT (pad, "Latency %" GST_TIME_FORMAT "/%" GST_TIME_FORMAT,
          GST_TIME_ARGS (min_latency), GST_TIME_ARGS (max_latency));

      // We are always live, the minimum latency is 1 frame and
      // the maximum latency is the complete buffer of frames.
      gst_query_set_latency (query, TRUE, min_latency, max_latency);
      break;
    }
    default:
      success = gst_pad_query_default (pad, parent, query);
      break;
  }

  return success;
}

static gboolean
video_pad_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean success = TRUE;

  GST_DEBUG_OBJECT (pad, "Received EVENT %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      qmmfsrc_video_pad_flush_buffers_queue (pad, TRUE);
      success = gst_pad_pause_task (pad);
      gst_event_unref (event);
      break;
    case GST_EVENT_FLUSH_STOP:
      qmmfsrc_video_pad_flush_buffers_queue (pad, FALSE);
      success = gst_pad_start_task (
          pad, (GstTaskFunction) video_pad_worker_task, pad, NULL);
      gst_event_unref (event);

      gst_segment_init (&GST_QMMFSRC_VIDEO_PAD (pad)->segment,
          GST_FORMAT_UNDEFINED);
      break;
    case GST_EVENT_EOS:
      // After EOS, we should not send any more buffers, even if there are
      // more requests coming in.
      qmmfsrc_video_pad_flush_buffers_queue (pad, TRUE);
      gst_event_unref (event);

      gst_segment_init (&GST_QMMFSRC_VIDEO_PAD (pad)->segment,
          GST_FORMAT_UNDEFINED);
      break;
    default:
      success = gst_pad_event_default (pad, parent, event);
      break;
  }
  return success;
}

static gboolean
video_pad_activate_mode (GstPad * pad, GstObject * parent, GstPadMode mode,
    gboolean active)
{
  gboolean success = FALSE;

  switch (mode) {
    case GST_PAD_MODE_PUSH:
      if (active) {
        qmmfsrc_video_pad_flush_buffers_queue (pad, FALSE);
        success = gst_pad_start_task (
            pad, (GstTaskFunction) video_pad_worker_task, pad, NULL);
      } else {
        qmmfsrc_video_pad_flush_buffers_queue (pad, TRUE);
        success = gst_pad_stop_task (pad);
      }
      break;
    default:
      break;
  }

  if (!success) {
    GST_ERROR_OBJECT (pad, "Failed to activate video pad task!");
    return success;
  }

  GST_DEBUG_OBJECT (pad, "Video Pad (%u) mode: %s",
      GST_QMMFSRC_VIDEO_PAD (pad)->index, active ? "ACTIVE" : "STOPED");

  // Call the default pad handler for activate mode.
  return gst_pad_activate_mode (pad, mode, active);
}

static void
video_pad_update_params (GstPad * pad, GstStructure * structure)
{
  GstQmmfSrcVideoPad *vpad = GST_QMMFSRC_VIDEO_PAD (pad);
  gint fps_n = 0, fps_d = 0;

  GST_QMMFSRC_VIDEO_PAD_LOCK (pad);

  gst_structure_get_int (structure, "width", &vpad->width);
  gst_structure_get_int (structure, "height", &vpad->height);
  gst_structure_get_fraction (structure, "framerate", &fps_n, &fps_d);

  vpad->duration = gst_util_uint64_scale_int (GST_SECOND, fps_d, fps_n);
  vpad->framerate = 1 / GST_TIME_AS_SECONDS (
      gst_guint64_to_gdouble (vpad->duration));

  if (gst_structure_has_name (structure, "video/x-raw")) {
    vpad->codec = GST_VIDEO_CODEC_NONE;
    vpad->format = gst_video_format_from_string (
        gst_structure_get_string (structure, "format"));
  } else if (gst_structure_has_name (structure, "video/x-bayer")) {
    const gchar *format = gst_structure_get_string (structure, "format");
    const gchar *bpp = gst_structure_get_string (structure, "bpp");

    vpad->codec = GST_VIDEO_CODEC_NONE;

    if (g_strcmp0 (bpp, "8") == 0)
      vpad->bpp = 8;
    else if (g_strcmp0 (bpp, "10") == 0)
      vpad->bpp = 10;
    else if (g_strcmp0 (bpp, "12") == 0)
      vpad->bpp = 12;
    else if (g_strcmp0 (bpp, "16") == 0)
      vpad->bpp = 16;

    if (g_strcmp0 (format, "bggr") == 0)
      vpad->format = GST_BAYER_FORMAT_BGGR;
    else if (g_strcmp0 (format, "rggb") == 0)
      vpad->format = GST_BAYER_FORMAT_RGGB;
    else if (g_strcmp0 (format, "gbrg") == 0)
      vpad->format = GST_BAYER_FORMAT_GBRG;
    else if (g_strcmp0 (format, "grbg") == 0)
      vpad->format = GST_BAYER_FORMAT_GRBG;
    else if (g_strcmp0 (format, "mono") == 0)
      vpad->format = GST_BAYER_FORMAT_MONO;
  } else {
    const gchar *profile, *level;

    profile = gst_structure_get_string (structure, "profile");
    gst_structure_set (vpad->params, "profile", G_TYPE_STRING, profile, NULL);

    level = gst_structure_get_string (structure, "level");
    gst_structure_set (vpad->params, "level", G_TYPE_STRING, level, NULL);

    vpad->format = GST_VIDEO_FORMAT_ENCODED;

    if (gst_structure_has_name (structure, "video/x-h264"))
      vpad->codec = GST_VIDEO_CODEC_H264;
    else if (gst_structure_has_name (structure, "video/x-h265"))
      vpad->codec = GST_VIDEO_CODEC_H265;
    else if (gst_structure_has_name (structure, "image/jpeg"))
      vpad->codec = GST_VIDEO_CODEC_JPEG;
  }

  if (gst_structure_has_field (structure, "compression")) {
    const gchar *compression =
        gst_structure_get_string (structure, "compression");

    vpad->compression = (g_strcmp0 (compression, "ubwc") == 0) ?
        GST_VIDEO_COMPRESSION_UBWC : GST_VIDEO_COMPRESSION_NONE;
  }

  GST_QMMFSRC_VIDEO_PAD_UNLOCK (pad);
}

GstPad *
qmmfsrc_request_video_pad (GstPadTemplate * templ, const gchar * name,
    const guint index)
{
  GstPad *srcpad = GST_PAD (g_object_new (
      GST_TYPE_QMMFSRC_VIDEO_PAD,
      "name", name,
      "direction", templ->direction,
      "template", templ,
      NULL
  ));
  g_return_val_if_fail (srcpad != NULL, NULL);

  GST_QMMFSRC_VIDEO_PAD (srcpad)->index = index;

  gst_pad_set_query_function (srcpad, GST_DEBUG_FUNCPTR (video_pad_query));
  gst_pad_set_event_function (srcpad, GST_DEBUG_FUNCPTR (video_pad_event));
  gst_pad_set_activatemode_function (
      srcpad, GST_DEBUG_FUNCPTR (video_pad_activate_mode));

  gst_pad_use_fixed_caps (srcpad);
  gst_pad_set_active (srcpad, TRUE);

  return srcpad;
}

void
qmmfsrc_release_video_pad (GstElement * element, GstPad * pad)
{
  gchar *padname = GST_PAD_NAME (pad);
  guint index = GST_QMMFSRC_VIDEO_PAD (pad)->index;

  gst_object_ref (pad);

  gst_child_proxy_child_removed (GST_CHILD_PROXY (element), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));
  gst_element_remove_pad (element, pad);
  gst_pad_set_active (pad, FALSE);

  gst_object_unref (pad);
}

gboolean
qmmfsrc_video_pad_fixate_caps (GstPad * pad)
{
  GstCaps *caps;
  GstStructure *structure;
  gint width = 0, height = 0;
  const GValue *framerate;

  // Get the negotiated caps between the pad and its peer.
  caps = gst_pad_get_allowed_caps (pad);
  g_return_val_if_fail (caps != NULL, FALSE);

  // Immediately return the fetched caps if they are fixed.
  if (gst_caps_is_fixed (caps)) {
    gst_pad_set_caps (pad, caps);
    video_pad_update_params (pad, gst_caps_get_structure (caps, 0));
    return TRUE;
  }

  // Capabilities are not fixated, fixate them.
  caps = gst_caps_make_writable (caps);
  structure = gst_caps_get_structure (caps, 0);

  gst_structure_get_int (structure, "width", &width);
  gst_structure_get_int (structure, "height", &height);
  framerate = gst_structure_get_value (structure, "framerate");

  if (!width) {
    gst_structure_set (structure, "width", G_TYPE_INT,
        DEFAULT_VIDEO_STREAM_WIDTH, NULL);
    GST_DEBUG_OBJECT (pad, "Width not set, using default value: %d",
        DEFAULT_VIDEO_STREAM_WIDTH);
  }

  if (!height) {
    gst_structure_set (structure, "height", G_TYPE_INT,
        DEFAULT_VIDEO_STREAM_HEIGHT, NULL);
    GST_DEBUG_OBJECT (pad, "Height not set, using default value: %d",
        DEFAULT_VIDEO_STREAM_HEIGHT);
  }

  if (!gst_value_is_fixed (framerate)) {
    gst_structure_fixate_field_nearest_fraction (structure, "framerate",
        DEFAULT_VIDEO_STREAM_FPS_NUM, DEFAULT_VIDEO_STREAM_FPS_DEN);
    GST_DEBUG_OBJECT (pad, "Framerate not set, using default value: %d/%d",
        DEFAULT_VIDEO_STREAM_FPS_NUM, DEFAULT_VIDEO_STREAM_FPS_DEN);
  }

  if (gst_structure_has_field (structure, "format")) {
    const gchar *format = gst_structure_get_string (structure, "format");
    gboolean isbayer = gst_structure_has_name (structure, "video/x-bayer");

    if (!format) {
      gst_structure_fixate_field_string (structure, "format",
          isbayer ? DEFAULT_VIDEO_BAYER_FORMAT : DEFAULT_VIDEO_RAW_FORMAT);
      GST_DEBUG_OBJECT (pad, "Format not set, using default value: %s",
          isbayer ? DEFAULT_VIDEO_BAYER_FORMAT : DEFAULT_VIDEO_RAW_FORMAT);
    }
  }

  if (gst_structure_has_field (structure, "bpp")) {
    const gchar *bpp = gst_structure_get_string (structure, "bpp");

    if (!bpp) {
      gst_structure_fixate_field_string (structure, "bpp",
          DEFAULT_VIDEO_BAYER_BPP);
      GST_DEBUG_OBJECT (pad, "BPP not set, using default value: %s",
          DEFAULT_VIDEO_BAYER_BPP);
    }
  }

  if (gst_structure_has_field (structure, "compression")) {
    const gchar *compression =
        gst_structure_get_string (structure, "compression");

    if (!compression) {
      gst_structure_fixate_field_string (structure, "compression",
            DEFAULT_VIDEO_RAW_COMPRESSION);
      GST_DEBUG_OBJECT (pad, "Compression not set, using default value: %s",
            DEFAULT_VIDEO_RAW_COMPRESSION);
    }
  }

  if (gst_structure_has_field (structure, "profile")) {
    const gchar *profile = gst_structure_get_string (structure, "profile");

    if (!profile) {
      if (gst_structure_has_name (structure, "video/x-h264")) {
        gst_structure_fixate_field_string (structure, "profile",
            DEFAULT_VIDEO_H264_PROFILE);
        GST_DEBUG_OBJECT (pad, "Codec profile not set, using default value: %s",
            DEFAULT_VIDEO_H264_PROFILE);
      } else if (gst_structure_has_name (structure, "video/x-h265")) {
        gst_structure_fixate_field_string (structure, "profile",
            DEFAULT_VIDEO_H265_PROFILE);
        GST_DEBUG_OBJECT (pad, "Codec profile not set, using default value: %s",
            DEFAULT_VIDEO_H265_PROFILE);
      } else {
        GST_DEBUG_OBJECT (pad, "Codec profile not required");
      }
    }
  }

  if (gst_structure_has_field (structure, "level")) {
    const gchar *level = gst_structure_get_string (structure, "level");

    if (!level) {
      if (gst_structure_has_name (structure, "video/x-h264")) {
        gst_structure_fixate_field_string (structure, "level",
            DEFAULT_VIDEO_H264_LEVEL);
        GST_DEBUG_OBJECT (pad, "Codec level not set, using default value: %s",
            DEFAULT_VIDEO_H264_LEVEL);
      } else if (gst_structure_has_name (structure, "video/x-h265")) {
        gst_structure_fixate_field_string (structure, "level",
            DEFAULT_VIDEO_H265_LEVEL);
        GST_DEBUG_OBJECT (pad, "Codec level not set, using default value: %s",
            DEFAULT_VIDEO_H265_LEVEL);
      } else {
        GST_DEBUG_OBJECT (pad, "Codec level not required");
      }
    }
  }

  gst_structure_remove_fields (structure, "stream-format", "alignment", NULL);

  caps = gst_caps_fixate (caps);
  gst_pad_set_caps (pad, caps);

  video_pad_update_params (pad, structure);
  return TRUE;
}

void
qmmfsrc_video_pad_flush_buffers_queue (GstPad * pad, gboolean flush)
{
  GST_INFO_OBJECT (pad, "Flushing buffer queue: %s", flush ? "TRUE" : "FALSE");

  gst_data_queue_set_flushing (GST_QMMFSRC_VIDEO_PAD (pad)->buffers, flush);
  gst_data_queue_flush (GST_QMMFSRC_VIDEO_PAD (pad)->buffers);
}

static void
video_pad_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec *pspec)
{
  GstQmmfSrcVideoPad *pad = GST_QMMFSRC_VIDEO_PAD (object);
  GstElement *parent = gst_pad_get_parent_element (GST_PAD (pad));
  const gchar *propname = g_param_spec_get_name (pspec);

  // Extract the state from the pad parent or in case there is no parent
  // use default value as parameters are being set upon object construction.
  GstState state = parent ? GST_STATE (parent) : GST_STATE_VOID_PENDING;

  // Decrease the pad parent reference count as it is not needed any more.
  if (parent != NULL)
    gst_object_unref (parent);

  if (!QMMFSRC_IS_PROPERTY_MUTABLE_IN_CURRENT_STATE (pspec, state)) {
    GST_WARNING_OBJECT (pad, "Property '%s' change not supported in %s state!",
        propname, gst_element_state_get_name (state));
    return;
  }

  GST_QMMFSRC_VIDEO_PAD_LOCK (pad);

  switch (property_id) {
    case PROP_VIDEO_SOURCE_INDEX:
      pad->srcidx = g_value_get_int (value);
      break;
    case PROP_VIDEO_FRAMERATE:
      pad->framerate = g_value_get_double (value);
      break;
    case PROP_VIDEO_BITRATE:
    case PROP_VIDEO_BITRATE_CONTROL:
    case PROP_VIDEO_QUANT_I_FRAMES:
    case PROP_VIDEO_QUANT_P_FRAMES:
    case PROP_VIDEO_QUANT_B_FRAMES:
    case PROP_VIDEO_MIN_QP:
    case PROP_VIDEO_MAX_QP:
    case PROP_VIDEO_MIN_QP_I_FRAMES:
    case PROP_VIDEO_MAX_QP_I_FRAMES:
    case PROP_VIDEO_MIN_QP_P_FRAMES:
    case PROP_VIDEO_MAX_QP_P_FRAMES:
    case PROP_VIDEO_MIN_QP_B_FRAMES:
    case PROP_VIDEO_MAX_QP_B_FRAMES:
    case PROP_VIDEO_IDR_INTERVAL:
      gst_structure_set_value (pad->params, propname, value);
      break;
    case PROP_VIDEO_CROP:
    {
      g_return_if_fail (gst_value_array_get_size (value) == 4);

      pad->crop.x = g_value_get_int (gst_value_array_get_value (value, 0));
      pad->crop.y = g_value_get_int (gst_value_array_get_value (value, 1));
      pad->crop.w = g_value_get_int (gst_value_array_get_value (value, 2));
      pad->crop.h = g_value_get_int (gst_value_array_get_value (value, 3));
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (pad, property_id, pspec);
      break;
  }

  GST_QMMFSRC_VIDEO_PAD_UNLOCK (pad);

  // Emit a 'notify' signal for the changed property.
  g_object_notify_by_pspec (G_OBJECT (pad), pspec);
}

static void
video_pad_get_property (GObject * object, guint property_id, GValue * value,
    GParamSpec * pspec)
{
  GstQmmfSrcVideoPad *pad = GST_QMMFSRC_VIDEO_PAD (object);

  GST_QMMFSRC_VIDEO_PAD_LOCK (pad);

  switch (property_id) {
    case PROP_VIDEO_SOURCE_INDEX:
      g_value_set_int (value, pad->srcidx);
      break;
    case PROP_VIDEO_FRAMERATE:
      g_value_set_double (value, pad->framerate);
      break;
    case PROP_VIDEO_BITRATE:
    case PROP_VIDEO_BITRATE_CONTROL:
    case PROP_VIDEO_QUANT_I_FRAMES:
    case PROP_VIDEO_QUANT_P_FRAMES:
    case PROP_VIDEO_QUANT_B_FRAMES:
    case PROP_VIDEO_MIN_QP:
    case PROP_VIDEO_MAX_QP:
    case PROP_VIDEO_MIN_QP_I_FRAMES:
    case PROP_VIDEO_MAX_QP_I_FRAMES:
    case PROP_VIDEO_MIN_QP_P_FRAMES:
    case PROP_VIDEO_MAX_QP_P_FRAMES:
    case PROP_VIDEO_MIN_QP_B_FRAMES:
    case PROP_VIDEO_MAX_QP_B_FRAMES:
    case PROP_VIDEO_IDR_INTERVAL:
      g_value_copy (gst_structure_get_value (pad->params,
           g_param_spec_get_name (pspec)), value);
      break;
    case PROP_VIDEO_CROP:
    {
      GValue val = G_VALUE_INIT;
      g_value_init (&val, G_TYPE_INT);

      g_value_set_int (&val, pad->crop.x);
      gst_value_array_append_value (value, &val);

      g_value_set_int (&val, pad->crop.y);
      gst_value_array_append_value (value, &val);

      g_value_set_int (&val, pad->crop.w);
      gst_value_array_append_value (value, &val);

      g_value_set_int (&val, pad->crop.h);
      gst_value_array_append_value (value, &val);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (pad, property_id, pspec);
      break;
  }

  GST_QMMFSRC_VIDEO_PAD_UNLOCK (pad);
}

static void
video_pad_finalize (GObject * object)
{
  GstQmmfSrcVideoPad *pad = GST_QMMFSRC_VIDEO_PAD (object);

  if (pad->buffers != NULL) {
    gst_data_queue_set_flushing (pad->buffers, TRUE);
    gst_data_queue_flush (pad->buffers);
    gst_object_unref (GST_OBJECT_CAST (pad->buffers));
    pad->buffers = NULL;
  }

  if (pad->params != NULL) {
    gst_structure_free (pad->params);
    pad->params = NULL;
  }

  G_OBJECT_CLASS (qmmfsrc_video_pad_parent_class)->finalize(object);
}

static gboolean
queue_is_full_cb (GstDataQueue * queue, guint visible, guint bytes,
    guint64 time, gpointer checkdata)
{
  // There won't be any condition limiting for the buffer queue size.
  return FALSE;
}

// QMMF Source video pad class initialization.
static void
qmmfsrc_video_pad_class_init (GstQmmfSrcVideoPadClass * klass)
{
  GObjectClass *gobject = G_OBJECT_CLASS (klass);

  gobject->get_property = GST_DEBUG_FUNCPTR (video_pad_get_property);
  gobject->set_property = GST_DEBUG_FUNCPTR (video_pad_set_property);
  gobject->finalize     = GST_DEBUG_FUNCPTR (video_pad_finalize);

  g_object_class_install_property (gobject, PROP_VIDEO_SOURCE_INDEX,
      g_param_spec_int ("source-index", "Source index",
          "Index of the source video pad to which this pad will be linked",
          -1, G_MAXINT, DEFAULT_PROP_SOURCE_INDEX,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject, PROP_VIDEO_FRAMERATE,
      g_param_spec_double ("framerate", "Framerate",
          "Target framerate in frames per second for displaying",
          0.0, 30.0, DEFAULT_PROP_FRAMERATE,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject, PROP_VIDEO_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate",
          "Target bitrate in bits per second for compressed streams",
          0, G_MAXUINT, DEFAULT_PROP_BITRATE,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject, PROP_VIDEO_BITRATE_CONTROL,
      g_param_spec_enum ("bitrate-control", "Bitrate Control",
          "Bitrate control method for compressed streams",
          GST_TYPE_VIDEO_PAD_CONTROL_RATE, DEFAULT_PROP_BITRATE_CONTROL,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject, PROP_VIDEO_QUANT_I_FRAMES,
      g_param_spec_uint ("quant-i-frames", "I-Frame Quantization",
          "Quantization parameter on I-frames for compressed streams",
          0, G_MAXUINT, DEFAULT_PROP_QUANT_I_FRAMES,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject, PROP_VIDEO_QUANT_P_FRAMES,
      g_param_spec_uint ("quant-p-frames", "P-Frame Quantization",
          "Quantization parameter on P-frames for compressed streams",
          0, G_MAXUINT, DEFAULT_PROP_QUANT_P_FRAMES,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject, PROP_VIDEO_QUANT_B_FRAMES,
      g_param_spec_uint ("quant-b-frames", "B-Frame Quantization",
          "Quantization parameter on B-frames for compressed streams",
          0, G_MAXUINT, DEFAULT_PROP_QUANT_B_FRAMES,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject, PROP_VIDEO_MIN_QP,
      g_param_spec_uint ("min-qp", "Min Quantization value",
          "Minimum QP value allowed during rate control for compressed "
          "streams",
          0, 51, DEFAULT_PROP_MIN_QP,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject, PROP_VIDEO_MAX_QP,
      g_param_spec_uint ("max-qp", "Max Quantization value",
          "Maximum QP value allowed during rate control for compressed "
          "streams",
          0, 51, DEFAULT_PROP_MAX_QP,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject, PROP_VIDEO_MIN_QP_I_FRAMES,
      g_param_spec_uint ("min-qp-i-frames", "I-Frame Min Quantization value",
          "Minimum QP value allowed on I-Frames during rate control for "
          "compressed streams",
          0, 51, DEFAULT_PROP_MIN_QP_I_FRAMES,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject, PROP_VIDEO_MAX_QP_I_FRAMES,
      g_param_spec_uint ("max-qp-i-frames", "I-Frame Max Quantization value",
          "Maximum QP value allowed on I-Frames during rate control for "
          "compressed streams",
          0, 51, DEFAULT_PROP_MAX_QP_I_FRAMES,
          (GParamFlags)(G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject, PROP_VIDEO_MIN_QP_P_FRAMES,
      g_param_spec_uint ("min-qp-p-frames", "P-Frame Min Quantization value",
          "Minimum QP value allowed on for P-Frames during rate control for "
          "compressed streams",
          0, 51, DEFAULT_PROP_MIN_QP_P_FRAMES,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject, PROP_VIDEO_MAX_QP_P_FRAMES,
      g_param_spec_uint ("max-qp-p-frames", "P-Frame Max Quantization value",
          "Maximum QP value allowed on P-Frames during rate control for "
          "compressed streams",
          0, 51, DEFAULT_PROP_MAX_QP_P_FRAMES,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject, PROP_VIDEO_MIN_QP_B_FRAMES,
      g_param_spec_uint ("min-qp-b-frames", "B-Frame Min Quantization value",
          "Minimum QP value allowed on B-Frames during rate control for "
          "compressed streams",
          0, 51, DEFAULT_PROP_MIN_QP_B_FRAMES,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject, PROP_VIDEO_MAX_QP_B_FRAMES,
      g_param_spec_uint ("max-qp-b-frames", "B-Frame Max Quantization value",
          "Maximum QP value allowed on B-Frames during rate control for "
          "compressed streams",
          0, 51, DEFAULT_PROP_MAX_QP_B_FRAMES,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject, PROP_VIDEO_IDR_INTERVAL,
      g_param_spec_uint ("idr-interval", "Instantaneous Decoder Refresh "
          "interval", "IDR interval for compressed streams",
          0, G_MAXUINT, DEFAULT_PROP_IDR_INTERVAL,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject, PROP_VIDEO_CROP,
      gst_param_spec_array ("crop", "Crop rectangle",
          "Crop rectangle ('<X, Y, WIDTH, HEIGHT>'). Applicable only for "
          "JPEG and YUY2 formats",
          g_param_spec_int ("value", "Crop Value",
              "One of X, Y, WIDTH or HEIGHT value.", 0, G_MAXINT, 0,
              G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

  GST_DEBUG_CATEGORY_INIT (qmmfsrc_video_pad_debug, "qtiqmmfsrc", 0,
      "QTI QMMF Source video pad");
}

// QMMF Source video pad initialization.
static void
qmmfsrc_video_pad_init (GstQmmfSrcVideoPad * pad)
{
  gst_segment_init (&pad->segment, GST_FORMAT_UNDEFINED);

  pad->index        = -1;
  pad->srcidx       = -1;

  pad->width        = -1;
  pad->height       = -1;
  pad->framerate    = 0.0;
  pad->format       = GST_VIDEO_FORMAT_UNKNOWN;
  pad->compression  = GST_VIDEO_COMPRESSION_NONE;
  pad->codec        = GST_VIDEO_CODEC_UNKNOWN;
  pad->params       = gst_structure_new_empty ("codec-params");
  pad->crop.x       = DEFAULT_PROP_CROP_X;
  pad->crop.y       = DEFAULT_PROP_CROP_Y;
  pad->crop.w       = DEFAULT_PROP_CROP_WIDTH;
  pad->crop.h       = DEFAULT_PROP_CROP_HEIGHT;

  pad->duration  = GST_CLOCK_TIME_NONE;

  pad->buffers   = gst_data_queue_new (queue_is_full_cb, NULL, NULL, pad);
}
