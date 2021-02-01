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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifndef GST_PACKAGE_ORIGIN
#   define GST_PACKAGE_ORIGIN "-"
#endif

#include <gst/gst.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "gstqticodec2venc.h"
#include "codec2wrapper.h"


GST_DEBUG_CATEGORY_STATIC (gst_qticodec2venc_debug);
#define GST_CAT_DEFAULT gst_qticodec2venc_debug

/* class initialization */
G_DEFINE_TYPE (Gstqticodec2venc, gst_qticodec2venc, GST_TYPE_VIDEO_ENCODER);

#define parent_class gst_qticodec2venc_parent_class
#define NANO_TO_MILLI(x)  x / 1000

enum
{
  PROP_0,
  PROP_SILENT
};

/* GstVideoEncoder base class method */
static gboolean gst_qticodec2venc_start (GstVideoEncoder* encoder);
static gboolean gst_qticodec2venc_stop (GstVideoEncoder* encoder);
static gboolean gst_qticodec2venc_set_format (GstVideoEncoder* encoder, GstVideoCodecState* state);
static GstFlowReturn gst_qticodec2venc_handle_frame (GstVideoEncoder* encoder, GstVideoCodecFrame* frame);
static GstFlowReturn gst_qticodec2venc_finish (GstVideoEncoder* encoder);
static gboolean gst_qticodec2venc_open (GstVideoEncoder* encoder);
static gboolean gst_qticodec2venc_close (GstVideoEncoder* encoder);
static gboolean gst_qticodec2venc_src_query (GstVideoEncoder* encoder, GstQuery* query);
static gboolean gst_qticodec2venc_sink_query (GstVideoEncoder* encoder, GstQuery* query);

static void gst_qticodec2venc_set_property (GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void gst_qticodec2venc_get_property (GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static void gst_qticodec2venc_finalize (GObject* object);

static gboolean gst_qticodec2venc_create_component(GstVideoEncoder* encoder);
static gboolean gst_qticodec2venc_destroy_component(GstVideoEncoder* encoder);
static void handle_video_event (const void* handle, EVENT_TYPE type, void* data);

static GstFlowReturn gst_qticodec2venc_encode (GstVideoEncoder* encoder, GstVideoCodecFrame* frame);
static GstFlowReturn gst_qticodec2venc_setup_output (GstVideoEncoder* encoder, GstVideoCodecState* state);
static GstFlowReturn gst_qticodec2venc_copy_output_buffer (GstVideoEncoder* encoder,  GstVideoCodecFrame* frame, const void* decodec_frame);

/* pad templates */
static GstStaticPadTemplate gst_qtivenc_src_template =
    GST_STATIC_PAD_TEMPLATE (GST_VIDEO_ENCODER_SRC_NAME ,
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        "video/x-h264," 
        "stream-format = (string) { byte-stream },"
        "alignment = (string) { au }"
        ";"
        "video/x-h265," 
        "stream-format = (string) { byte-stream },"
        "alignment = (string) { au }"
        ";"
        "video/x-vp8"
      )
    );

static GstStaticPadTemplate gst_qtivenc_sink_template =
GST_STATIC_PAD_TEMPLATE (GST_VIDEO_ENCODER_SINK_NAME,
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        "video/x-raw, "
        "format = (string) NV12, "
        "width  = (int) [ 32, 4096 ], "
        "height = (int) [ 32, 4096 ]" )
    );

static ConfigParams
make_resolution_param (guint32 width, guint32 height, gboolean isInput) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.isInput = isInput;
  param.resolution.width = width;
  param.resolution.height = height;

  return param;
}

static ConfigParams
make_pixelFormat_param (guint32 fmt, gboolean isInput) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.isInput = isInput;
  param.val.u32 = fmt;

  return param;
}

static ConfigParams
make_interlace_param (INTERLACE_MODE_TYPE mode, gboolean isInput) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.isInput = isInput;
  param.interlaceMode.type = mode;

  return param;
}

static gchar*
gst_to_c2_streamformat (GstStructure* structure) {
  gchar *ret = NULL;
  
  if (gst_structure_has_name (structure, "video/x-h264")) {
    ret = g_strdup("c2.qti.avc.encoder");
  }
  else if (gst_structure_has_name (structure, "video/hevc")) {
    ret = g_strdup("c2.qti.hevc.encoder");
  }
  else if (gst_structure_has_name (structure, "video/mpeg2")) {
    ret = g_strdup("c2.qti.mpeg2.encoder");
  }

  return ret;
}

static guint32
gst_to_c2_pixelformat (GstVideoFormat format, gboolean compressed) {
  guint32 result = 0;

  switch(format) {
    case GST_VIDEO_FORMAT_NV12 :{
      if (!compressed) {
        result = PIXEL_FORMAT_NV12_LINEAR;
      } else{
        result = PIXEL_FORMAT_NV12_UBWC;
      }
      break;
    }
    default:{
      break;
    }
  }

  return result;
}

static gboolean
gst_qticodec2venc_create_component (GstVideoEncoder* encoder) {
  gboolean ret = FALSE;
  Gstqticodec2venc *enc = GST_QTICODEC2VENC (encoder);

  GST_DEBUG_OBJECT (enc, "create_component");

  if (enc->comp_store) {

    ret = c2componentStore_createComponent(enc->comp_store, enc->streamformat, &enc->comp);
    if (ret ==  FALSE) {
       GST_DEBUG_OBJECT (enc, "Failed to create component");
    }

    ret = c2componentStore_createInterface(enc->comp_store, enc->streamformat, &enc->comp_intf);
    if (ret ==  FALSE) {
       GST_DEBUG_OBJECT (enc, "Failed to create component interface");
    }

    ret = c2component_setListener(enc->comp, encoder, handle_video_event, BLOCK_MODE_MAY_BLOCK);
    if (ret ==  FALSE) {
       GST_DEBUG_OBJECT (enc, "Failed to set event handler");
    }

    ret = c2component_createBlockpool(enc->comp, BUFFER_POOL_BASIC_LINEAR);
    if (ret ==  FALSE) {
       GST_DEBUG_OBJECT (enc, "Failed to create linear pool");
    }

    ret = c2component_createBlockpool(enc->comp, BUFFER_POOL_BASIC_GRAPHIC);
    if (ret == FALSE) {
      GST_DEBUG_OBJECT (enc, "Failed to create graphics pool");
    }
  }
  else {
    GST_DEBUG_OBJECT (enc, "Component store is Null");
  }

  return ret;
}

static gboolean
gst_qticodec2venc_destroy_component (GstVideoEncoder* encoder) {
  gboolean ret = FALSE;
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);

  GST_DEBUG_OBJECT (enc, "destroy_component");

  if (enc->comp_intf) {
    c2componentInterface_delete(enc->comp_intf);
  }

  if (enc->comp) {
    c2component_delete(enc->comp);
  }

  return ret;
}

static GstFlowReturn
gst_qticodec2venc_setup_output (GstVideoEncoder* encoder, GstVideoCodecState* state) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
  GstFlowReturn ret = GST_FLOW_OK;
  GstCaps *outcaps;

  GST_DEBUG_OBJECT (enc, "setup_output");

  if (enc->output_state) {
    gst_video_codec_state_unref (enc->output_state);
  }

  outcaps = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (encoder));
  if (outcaps) {
    GstStructure *structure;
    gchar* streamformat;

    if (gst_caps_is_empty (outcaps)) {
      gst_caps_unref (outcaps);
      GST_ERROR_OBJECT (enc, "Unsupported format in caps: %" GST_PTR_FORMAT, outcaps);
      return GST_FLOW_ERROR;
    }

    outcaps = gst_caps_make_writable (outcaps);
    outcaps = gst_caps_fixate (outcaps);
    structure = gst_caps_get_structure (outcaps, 0);

    streamformat = gst_to_c2_streamformat (structure);
    if (!streamformat) {
      GST_ERROR_OBJECT (enc, "Unsupported format in caps: %" GST_PTR_FORMAT, outcaps);
      gst_caps_unref (outcaps);
      return GST_FLOW_ERROR;
    }

    enc->streamformat = streamformat;
    if (!gst_qticodec2venc_create_component (encoder)){
      GST_ERROR_OBJECT(enc, "Failed to create component");
    }

    enc->output_state = gst_video_encoder_set_output_state (encoder, outcaps, state);
    enc->output_setup = TRUE;
  }

  return ret;
}

/* Called when the element starts processing. Opening external resources. */
static gboolean
gst_qticodec2venc_start (GstVideoEncoder* encoder) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);

  GST_DEBUG_OBJECT (enc, "start");

  return TRUE;
}

/* Called when the element stops processing. Close external resources. */
static gboolean
gst_qticodec2venc_stop (GstVideoEncoder* encoder) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (enc, "stop");

  /* Stop the component */
  if (enc->comp) {
    ret = c2component_stop(enc->comp);
  }
  
  if (enc->input_state) {
    gst_video_codec_state_unref (enc->input_state);
    enc->input_state = NULL;
  }

  if (enc->output_state) {
    gst_video_codec_state_unref (enc->output_state);
    enc->output_state = NULL;
  }

  return ret;
}

/* Dispatch any pending remaining data at EOS. Class can refuse to decode new data after. */
static GstFlowReturn
gst_qticodec2venc_finish (GstVideoEncoder* encoder) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC(encoder);
  gint64 timeout;
  FLAG_TYPE inputFrameFlag = 0;

  GST_DEBUG_OBJECT (enc, "finish");

  return GST_FLOW_OK;
}

/* Called to inform the caps describing input video data that encoder is about to receive.
  Might be called more than once, if changing input parameters require reconfiguration.*/
static gboolean
gst_qticodec2venc_set_format (GstVideoEncoder* encoder, GstVideoCodecState* state) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
  GstStructure* structure;
  const gchar* mode;
  const gchar *fmt;
  gint retval = 0;
  gint width = 0;
  gint height = 0;
  GstVideoFormat input_format = GST_VIDEO_FORMAT_UNKNOWN;
  GstVideoInterlaceMode interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
  INTERLACE_MODE_TYPE c2interlace_mode = INTERLACE_MODE_PROGRESSIVE;
  GHashTable* config = NULL;
  ConfigParams resolution;
  ConfigParams interlace;
  ConfigParams pixelformat;

  GST_DEBUG_OBJECT (enc, "set_format");

  structure = gst_caps_get_structure (state->caps, 0);
  retval = gst_structure_get_int (structure, "width", &width);
  retval &= gst_structure_get_int (structure, "height", &height);
  if (!retval) {
    goto error_res;
  }

  fmt = gst_structure_get_string (structure, "format");
  if (fmt) {
    input_format = gst_video_format_from_string(fmt);
    if (input_format == GST_VIDEO_FORMAT_UNKNOWN) {
      goto error_format;
    }
  }

  if (enc->input_setup) {
    /* Already setup, check to see if something has changed on input caps... */
    if ((enc->width == width) && (enc->height == height)) {
      goto done;                /* Nothing has changed */
    } else {
      gst_qticodec2venc_stop (encoder);
    }
  }

  if ((mode = gst_structure_get_string (structure, "interlace-mode"))) {
    if (g_str_equal ("progressive", mode)) {
      interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
      c2interlace_mode = INTERLACE_MODE_PROGRESSIVE;
    } else if(g_str_equal ("interleaved", mode)) {
      interlace_mode = GST_VIDEO_INTERLACE_MODE_INTERLEAVED;
      c2interlace_mode = INTERLACE_MODE_INTERLEAVED_TOP_FIRST;
    } else if(g_str_equal ("mixed", mode)) {
      interlace_mode = GST_VIDEO_INTERLACE_MODE_MIXED;
      c2interlace_mode = INTERLACE_MODE_INTERLEAVED_TOP_FIRST;
    } else if(g_str_equal ("fields", mode)) {
      interlace_mode = GST_VIDEO_INTERLACE_MODE_FIELDS;
      c2interlace_mode = INTERLACE_MODE_FIELD_TOP_FIRST;
    }
  }

  enc->width = width;
  enc->height = height;
  enc->interlace_mode = interlace_mode;
  enc->input_format = input_format;

  if (enc->input_state) {
    gst_video_codec_state_unref (enc->input_state);
  }

  enc->input_state = gst_video_codec_state_ref (state);

  if (GST_FLOW_OK != gst_qticodec2venc_setup_output(encoder, state)) {
    goto error_output;
  }

  config = g_hash_table_new(g_str_hash, g_str_equal);

  resolution = make_resolution_param(width, height, TRUE);
  g_hash_table_insert(config, CONFIG_FUNCTION_KEY_RESOLUTION, &resolution);

  interlace = make_interlace_param(c2interlace_mode, TRUE);
  g_hash_table_insert(config, CONFIG_FUNCTION_KEY_INTERLACE, &interlace);

  pixelformat = make_pixelFormat_param(gst_to_c2_pixelformat(input_format, FALSE), TRUE);
  g_hash_table_insert(config, CONFIG_FUNCTION_KEY_PIXELFORMAT, &pixelformat);

  if (!c2componentInterface_config(
        enc->comp_intf,
        config,
        BLOCK_MODE_MAY_BLOCK)) {
      GST_ERROR_OBJECT (enc, "Failed to set output color");
      //g_hash_table_destroy (config);
      // goto error_config;
  }

  g_hash_table_destroy (config);

  if (!c2component_start(enc->comp)) {
    GST_DEBUG_OBJECT (enc, "Failed to start component");
    goto error_config;
  }

done:
  enc->input_setup = TRUE;
  return TRUE;

  /* Errors */
error_format:
  {
    GST_ERROR_OBJECT (enc, "Unsupported format in caps: %" GST_PTR_FORMAT, state->caps);
    return FALSE;
  }
error_res:
  {
    GST_ERROR_OBJECT (enc, "Unable to get width/height value");
    return FALSE;
  }
error_output:
  {
    GST_ERROR_OBJECT (enc, "Unable to set output state");
    return FALSE;
  }
error_config:
  {
    GST_ERROR_OBJECT (enc, "Unable to configure the component");
    return FALSE;
  }

  return TRUE;
}

/* Called when the element changes to GST_STATE_READY */
static gboolean
gst_qticodec2venc_open (GstVideoEncoder* encoder) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (enc, "open");

  /* Create component store */
  enc->comp_store = c2componentStore_create();

  return ret;
}

/* Called when the element changes to GST_STATE_NULL */
static gboolean
gst_qticodec2venc_close (GstVideoEncoder* encoder) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
 
  GST_DEBUG_OBJECT (enc, "qticodec2venc_close");

  if (enc->input_state) {
    gst_video_codec_state_unref (enc->input_state);
    enc->input_state = NULL;
  }

  if (enc->output_state) {
    gst_video_codec_state_unref (enc->output_state);
    enc->output_state = NULL;
  }

  if (!gst_qticodec2venc_destroy_component(encoder)){
    GST_DEBUG_OBJECT (enc, "Failed to delete component");
  }

  return TRUE;
}

/* Called whenever a input frame from the upstream is sent to encoder */
static GstFlowReturn
gst_qticodec2venc_handle_frame (GstVideoEncoder* encoder, GstVideoCodecFrame* frame) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT (enc, "handle_frame");

  if (!enc->input_setup) {
    return GST_FLOW_OK;
  }

  if (!enc->output_setup) {
    return GST_FLOW_ERROR;
  }

  GST_DEBUG ("Frame number : %d, Distance from Sync : %d, Ref count : %d, Presentation timestamp : %" GST_TIME_FORMAT,
    frame->system_frame_number, frame->distance_from_sync, frame->ref_count, GST_TIME_ARGS (frame->pts));

  /* Encode frame */
  if (frame) {
    return gst_qticodec2venc_encode (encoder, frame);
  }
  else {
    return GST_FLOW_EOS;
  }
}

static gboolean
gst_qticodec2venc_src_query (GstVideoEncoder* encoder, GstQuery* query) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
  GstPad* pad = GST_VIDEO_DECODER_SRC_PAD (encoder);
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (enc, "src_query of type '%s'", gst_query_type_get_name (GST_QUERY_TYPE (query)));

  switch (GST_QUERY_TYPE (query)) {
    default:
      ret = GST_VIDEO_ENCODER_CLASS (parent_class)->src_query (encoder, query);
      break;
  }

  return ret;
}

static gboolean
gst_qticodec2venc_sink_query (GstVideoEncoder* encoder, GstQuery* query) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
  GstPad* pad = GST_VIDEO_DECODER_SINK_PAD (encoder);
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (enc, "sink_query of type '%s'",gst_query_type_get_name (GST_QUERY_TYPE (query)));

  switch (GST_QUERY_TYPE (query)) {
    default:
      ret = GST_VIDEO_ENCODER_CLASS (parent_class)->sink_query (encoder, query);
      break;
  }

  return ret;
}

static GstFlowReturn
gst_qticodec2venc_add_meta (GstVideoEncoder* encoder, GstBuffer* out_buf) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_qticodec2venc_copy_output_buffer (GstVideoEncoder* encoder,  GstVideoCodecFrame* frame, const void* encodec_frame) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
  GstFlowReturn result = GST_FLOW_OK;

  GST_DEBUG_OBJECT (enc, "qticodec2venc_copy_output_buffer");

  return result;
}

/* Push decoded frame to downstream element */
static GstFlowReturn
push_frame_downstream(GstVideoEncoder* encoder, BufferDescriptor* encode_buf) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT (enc, "push_frame_downstream");

  return ret;
}

/* Handle event from Codec2 */
static void
handle_video_event (const void* handle, EVENT_TYPE type, void* data) {
  GstVideoEncoder *encoder = (GstVideoEncoder *) handle;
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (enc, "handle_video_event");

  switch(type) {
    case EVENT_OUTPUTS_DONE: {
      GST_DEBUG_OBJECT (enc, "revent for capture buffer type");
      ret = push_frame_downstream (encoder, (BufferDescriptor*)data);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT (enc, "Failed to push frame downstream");
      }
      break;
    }
    case EVENT_TRIPPED: {
      GST_ERROR_OBJECT (enc, "Failed to apply configuration setting(%d)", *(gint32*)data);
      break;
    }
    case EVENT_ERROR: {
      GST_ERROR_OBJECT (enc, "Something un-expected happened(%d)", *(gint32*)data);
      break;
    }
    default:{
      GST_ERROR_OBJECT (enc, "Invalid Event(%d)", type);
    }
  }
}

/* Push frame to Codec2 */
static GstFlowReturn
gst_qticodec2venc_encode (GstVideoEncoder* encoder, GstVideoCodecFrame* frame) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
  GstVideoFrame video_frame;

  GST_DEBUG_OBJECT (enc, "encode");

  if (frame) {
    GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

    gst_video_frame_map (&video_frame, &enc->input_state->info, frame->input_buffer, GST_MAP_READ);
    gst_video_frame_unmap (&video_frame);

    GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  }

  /* Keep track of queued frame */
  enc->queued_frame[(enc->frame_index) % MAX_QUEUED_FRAME] =  frame->system_frame_number;

  return GST_FLOW_OK;
}

static void
gst_qticodec2venc_set_property (GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (object);

  GST_DEBUG_OBJECT (enc, "qticodec2venc_set_property");

  switch (prop_id) {
    case PROP_SILENT:
      enc->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_qticodec2venc_get_property (GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (object);

  GST_DEBUG_OBJECT (enc, "qticodec2venc_get_property");

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, enc->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Called during object destruction process */
static void
gst_qticodec2venc_finalize (GObject *object) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (object);

  GST_DEBUG_OBJECT (enc, "finalize");

  g_mutex_clear (&enc->pending_lock);
  g_cond_clear (&enc->pending_cond);

  g_free (enc->streamformat);

  if (enc->streamformat) {
    enc->streamformat = NULL;
  }

  gst_qticodec2venc_destroy_component(GST_VIDEO_ENCODER(enc));

  if (enc->comp_store) {
    c2componentStore_delete(enc->comp_store);
  }

  /* Lastly chain up to the parent class */
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* entry point to initialize the plug-in
 * register the plugin
 */
static gboolean
plugin_init (GstPlugin* qticodec2venc) {
  /* debug category for fltering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_qticodec2venc_debug, "qticodec2venc",
      0, "QTI GST codec2.0 video encoder");

  return gst_element_register (qticodec2venc, "qticodec2venc", GST_RANK_PRIMARY + 1,
      GST_TYPE_QTICODEC2VENC);
}

/* Initialize the qticodec2venc's class */
static void
gst_qticodec2venc_class_init (Gstqticodec2vencClass* klass) {
  GstVideoEncoderClass* video_encoder_class = GST_VIDEO_ENCODER_CLASS (klass);
  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass* gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_qtivenc_src_template));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_qtivenc_sink_template));

  /* Set GObject class property*/      
  gobject_class->set_property = gst_qticodec2venc_set_property;
  gobject_class->get_property = gst_qticodec2venc_get_property;
  gobject_class->finalize = gst_qticodec2venc_finalize;

  /* Add property to this class */
  g_object_class_install_property (G_OBJECT_CLASS(klass), PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  video_encoder_class->start = GST_DEBUG_FUNCPTR (gst_qticodec2venc_start);
  video_encoder_class->stop = GST_DEBUG_FUNCPTR (gst_qticodec2venc_stop);
  video_encoder_class->set_format = GST_DEBUG_FUNCPTR (gst_qticodec2venc_set_format);
  video_encoder_class->handle_frame =  GST_DEBUG_FUNCPTR (gst_qticodec2venc_handle_frame);
  video_encoder_class->finish = GST_DEBUG_FUNCPTR (gst_qticodec2venc_finish);
  video_encoder_class->open = GST_DEBUG_FUNCPTR (gst_qticodec2venc_open);
  video_encoder_class->close = GST_DEBUG_FUNCPTR (gst_qticodec2venc_close);
  video_encoder_class->src_query = GST_DEBUG_FUNCPTR (gst_qticodec2venc_src_query);
  video_encoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_qticodec2venc_sink_query);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass), 
   "Codec2 video encoder", "Encoder/Video", "Video Encoder based on Codec2.0", "QTI");
}

/* Invoked during object instantiation (equivalent C++ constructor). */
static void
gst_qticodec2venc_init (Gstqticodec2venc* enc) {
  GstVideoEncoder* encoder = (GstVideoEncoder *) enc;

  enc->comp_store = NULL;
  enc->comp = NULL;
  enc->comp_intf = NULL;
  enc->input_setup = FALSE;
  enc->output_setup = FALSE;
  enc->width = 0;
  enc->height = 0;
  enc->frame_index = 0;

  memset(enc->queued_frame, 0, MAX_QUEUED_FRAME);

  g_cond_init (&enc->pending_cond);
  g_mutex_init(&enc->pending_lock);

  enc->silent = FALSE;
}
  
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    qticodec2venc,
    "QTI GST Codec2.0 Video Encoder",
    plugin_init,
    VERSION,
    GST_LICENSE_UNKNOWN,
    PACKAGE_NAME,
    GST_PACKAGE_ORIGIN
)
