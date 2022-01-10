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

#include "gstqticodec2vdec.h"
#include "gstqticodec2vdecbufferpool.h"
#include "codec2wrapper.h"
#include <dlfcn.h>
#include <libdrm/drm_fourcc.h>

GST_DEBUG_CATEGORY_STATIC (gst_qticodec2vdec_debug);
#define GST_CAT_DEFAULT gst_qticodec2vdec_debug

/* class initialization */
G_DEFINE_TYPE (Gstqticodec2vdec, gst_qticodec2vdec, GST_TYPE_VIDEO_DECODER);

#define parent_class gst_qticodec2vdec_parent_class
#define NANO_TO_MILLI(x)  ((x) / 1000)
#define EOS_WAITING_TIMEOUT 5
#define QCODEC2_MIN_OUTBUFFERS 6

#define GST_QTI_CODEC2_DEC_OUTPUT_PICTURE_ORDER_MODE_DEFAULT    (0xffffffff)
#define GST_QTI_CODEC2_DEC_LOW_LATENCY_MODE_DEFAULT             (FALSE)
#define GST_QTI_CODEC2_DEC_MAP_OUTBUF_DEFAULT                   (0xffffffff)

/* Function will be named qticodec2vdec_qdata_quark() */
static G_DEFINE_QUARK (QtiCodec2DecoderQuark, qticodec2vdec_qdata);
static G_DEFINE_QUARK (QtiCodec2C2BufQuark, qticodec2_c2buf_qdata);
static G_DEFINE_QUARK (FBufModifierQuark, gst_fbuf_modifier_qdata);


enum
{
  PROP_0,
  PROP_SILENT,
  PROP_OUTPUT_PICTURE_ORDER,
  PROP_LOW_LATENCY,
  PROP_MAP_OUTBUF,
};

/* GstVideoDecoder base class method */
static gboolean gst_qticodec2vdec_start (GstVideoDecoder * decoder);
static gboolean gst_qticodec2vdec_stop (GstVideoDecoder * decoder);
static gboolean gst_qticodec2vdec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_qticodec2vdec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_qticodec2vdec_finish (GstVideoDecoder * decoder);
static gboolean gst_qticodec2vdec_open (GstVideoDecoder * decoder);
static gboolean gst_qticodec2vdec_close (GstVideoDecoder * decoder);
static gboolean gst_qticodec2vdec_src_query (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_qticodec2vdec_sink_query (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_qticodec2vdec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query);

static void gst_qticodec2vdec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_qticodec2vdec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_qticodec2vdec_finalize (GObject * object);

static gboolean gst_qticodec2vdec_create_component (GstVideoDecoder * decoder);
static gboolean gst_qticodec2vdec_destroy_component (GstVideoDecoder * decoder);
static void handle_video_event (const void *handle, EVENT_TYPE type,
    void *data);

static GstFlowReturn gst_qticodec2vdec_decode (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_qticodec2vdec_setup_output (GstVideoDecoder * decoder,
    GPtrArray * config);
static GstBuffer *gst_qticodec2vdec_wrap_output_buffer (GstVideoDecoder *
    decoder, BufferDescriptor * buffer);
static void gst_video_decoder_buffer_release (GstStructure * structure);
static gboolean gst_qticodec2vdec_caps_has_feature (const GstCaps * caps,
    const gchar * partten);

/* pad templates */
static GstStaticPadTemplate gst_qtivdec_sink_template =
    GST_STATIC_PAD_TEMPLATE (GST_VIDEO_DECODER_SINK_NAME,
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264,"
        "stream-format = (string) { byte-stream },"
        "alignment = (string) { au }"
        ";"
        "video/x-h265,"
        "stream-format = (string) { byte-stream },"
        "alignment = (string) { au }"
        ";"
        "video/x-vp8"
        ";" "video/x-vp9" ";" "video/mpeg," "mpegversion = (int)2")
    );

static GstStaticPadTemplate gst_qtivdec_src_template =
    GST_STATIC_PAD_TEMPLATE (GST_VIDEO_DECODER_SRC_NAME,
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (QTICODEC2VDEC_RAW_CAPS_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_DMABUF, "{ NV12 }")
        ";" QTICODEC2VDEC_RAW_CAPS ("{ NV12 }")
    )
    );

static gboolean
caps_has_compression (const GstCaps * caps, const gchar * compression)
{
  GstStructure *structure = NULL;
  const gchar *string = NULL;

  structure = gst_caps_get_structure (caps, 0);
  string = gst_structure_has_field (structure, "compression") ?
      gst_structure_get_string (structure, "compression") : NULL;

  return (g_strcmp0 (string, compression) == 0) ? TRUE : FALSE;
}

static void
modifier_free (gpointer p_modifier)
{
  if (p_modifier) {
    g_slice_free (guint64, p_modifier);
    GST_DEBUG ("modifier_free(%p) val 0x%lx called", p_modifier,
        *(guint64 *) p_modifier);
  } else {
    GST_ERROR ("invalid modifier");
  }

  return;
}

static ConfigParams
make_resolution_param (guint32 width, guint32 height, gboolean isInput)
{
  ConfigParams param;

  memset (&param, 0, sizeof (ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_RESOLUTION;
  param.isInput = isInput;
  param.resolution.width = width;
  param.resolution.height = height;

  return param;
}

static ConfigParams
make_pixelFormat_param (guint32 fmt, gboolean isInput)
{
  ConfigParams param;

  memset (&param, 0, sizeof (ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_PIXELFORMAT;
  param.isInput = isInput;
  param.pixelFormat.fmt = fmt;

  return param;
}

static ConfigParams
make_interlace_param (INTERLACE_MODE_TYPE mode, gboolean isInput)
{
  ConfigParams param;

  memset (&param, 0, sizeof (ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_INTERLACE;
  param.isInput = isInput;
  param.interlaceMode.type = mode;

  return param;
}

static ConfigParams
make_output_picture_order_param (guint output_picture_order_mode)
{
  ConfigParams param;

  memset (&param, 0, sizeof (ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_OUTPUT_PICTURE_ORDER_MODE;
  param.output_picture_order_mode = output_picture_order_mode;

  return param;
}

static ConfigParams
make_low_latency_param (gboolean low_latency_mode)
{
  ConfigParams param;

  memset (&param, 0, sizeof (ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_DEC_LOW_LATENCY;
  param.low_latency_mode = low_latency_mode;

  return param;
}

static gchar *
gst_to_c2_streamformat (GstVideoDecoder * decoder, GstStructure * s,
    gboolean low_latency)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);
  gchar *str = NULL;
  gchar *concat_str = NULL;
  gchar *str_low_latency = g_strdup (".low_latency");
  gboolean supported = FALSE;
  gint mpegversion = 0;

  if (gst_structure_has_name (s, "video/x-h264")) {
    str = g_strdup ("c2.qti.avc.decoder");
  } else if (gst_structure_has_name (s, "video/x-h265")) {
    str = g_strdup ("c2.qti.hevc.decoder");
  } else if (gst_structure_has_name (s, "video/x-vp8")) {
    str = g_strdup ("c2.qti.vp8.decoder");
  } else if (gst_structure_has_name (s, "video/x-vp9")) {
    str = g_strdup ("c2.qti.vp9.decoder");
  } else if (gst_structure_has_name (s, "video/mpeg")) {
    if (gst_structure_get_int (s, "mpegversion", &mpegversion)) {
      if (mpegversion == 2) {
        str = g_strdup ("c2.qti.mpeg2.decoder");
      }
    }
  }

  if (low_latency) {
    concat_str = g_strconcat (str, str_low_latency, NULL);
    supported =
        c2componentStore_isComponentSupported (dec->comp_store, concat_str);

    if (supported) {
      if (str)
        g_free (str);
      str = concat_str;
    } else {
      g_free (concat_str);
    }
  }

  if (str_low_latency)
    g_free (str_low_latency);

  return str;
}

static guint32
gst_to_c2_pixelformat (GstVideoDecoder * decoder, GstVideoFormat format)
{
  guint32 result = 0;
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);

  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
      if (dec->is_ubwc) {
        result = PIXEL_FORMAT_NV12_UBWC;
      } else {
        result = PIXEL_FORMAT_NV12_LINEAR;
      }
      break;
    default:
      result = PIXEL_FORMAT_NV12_UBWC;
      GST_WARNING_OBJECT (dec,
          "Invalid pixel format(%d), fallback to NV12 UBWC", format);
      break;
  }

  GST_DEBUG_OBJECT (dec, "to_c2_pixelformat (%s), c2 format: %d",
      gst_video_format_to_string (format), result);

  return result;
}

static gboolean
gst_qticodec2vdec_create_component (GstVideoDecoder * decoder)
{
  gboolean ret = FALSE;
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);

  GST_DEBUG_OBJECT (dec, "create component");

  if (dec->comp_store) {
    ret =
        c2componentStore_createComponent (dec->comp_store, dec->streamformat,
        &dec->comp);
    if (ret == TRUE) {
      dec->comp_intf = c2component_intf (dec->comp);
      if (dec->comp_intf) {
        ret =
            c2component_setListener (dec->comp, decoder, handle_video_event,
            BLOCK_MODE_MAY_BLOCK);
        if (ret == TRUE) {
          ret =
              c2component_createBlockpool (dec->comp, BUFFER_POOL_BASIC_LINEAR);
          if (ret == FALSE) {
            GST_ERROR_OBJECT (dec, "Failed to create linear pool");
          }
        } else {
          GST_ERROR_OBJECT (dec, "Failed to set event handler");
        }
      } else {
        GST_ERROR_OBJECT (dec, "Failed to create interface");
      }
    } else {
      GST_ERROR_OBJECT (dec, "Failed to create component");
    }
  } else {
    GST_ERROR_OBJECT (dec, "Component store is Null");
  }

  return ret;
}

static gboolean
gst_qticodec2vdec_destroy_component (GstVideoDecoder * decoder)
{
  gboolean ret = TRUE;
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);

  GST_DEBUG_OBJECT (dec, "destroy_component");

  if (dec->comp) {
    c2component_delete (dec->comp);
  }

  return ret;
}

static GstFlowReturn
gst_qticodec2vdec_setup_output (GstVideoDecoder * decoder, GPtrArray * config)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);
  GstVideoAlignment align;
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoFormat output_format = GST_VIDEO_FORMAT_NV12;
  ConfigParams pixelformat;

  GstCaps *templ_caps, *intersection = NULL;
  GstStructure *s;
  const gchar *format_str;
  gboolean actual_map = FALSE;

  /* Set decoder output format to NV12 by default */
  dec->output_state =
      gst_video_decoder_set_output_state (decoder,
      output_format, dec->width, dec->height, dec->input_state);

  /* state->caps should be NULL */
  if (dec->output_state->caps) {
    gst_caps_unref (dec->output_state->caps);
  }

  /* Fixate decoder output caps */
  templ_caps =
      gst_pad_get_pad_template_caps (GST_VIDEO_DECODER_SRC_PAD (decoder));
  intersection =
      gst_pad_peer_query_caps (GST_VIDEO_DECODER_SRC_PAD (decoder), templ_caps);
  gst_caps_unref (templ_caps);

  GST_DEBUG_OBJECT (dec, "Allowed downstream caps: %" GST_PTR_FORMAT,
      intersection);

  if (gst_caps_is_empty (intersection)) {
    gst_caps_unref (intersection);
    GST_ERROR_OBJECT (dec, "Empty caps");
    goto error_setup_output;
  }

  /* Fixate color format */
  intersection = gst_caps_truncate (intersection);
  intersection = gst_caps_fixate (intersection);
  GST_DEBUG_OBJECT (dec, "intersection caps: %" GST_PTR_FORMAT, intersection);

  s = gst_caps_get_structure (intersection, 0);
  format_str = gst_structure_get_string (s, "format");
  dec->is_ubwc = caps_has_compression (intersection, "ubwc");
  GST_DEBUG_OBJECT (dec, "Fixed color format:%s, UBWC:%d", format_str,
      dec->is_ubwc);

  if (!format_str || (output_format = gst_video_format_from_string (format_str))
      == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ERROR_OBJECT (dec, "Invalid caps: %" GST_PTR_FORMAT, intersection);
    gst_caps_unref (intersection);
    goto error_setup_output;
  }

  GST_DEBUG_OBJECT (dec,
      "Set decoder output state: color format: %d, width: %d, height: %d",
      output_format, dec->width, dec->height);

  /* Fill actual width/height into output caps */
  GValue g_width = { 0, };
  GValue g_height = { 0, };
  g_value_init (&g_width, G_TYPE_INT);
  g_value_set_int (&g_width, dec->width);

  g_value_init (&g_height, G_TYPE_INT);
  g_value_set_int (&g_height, dec->height);
  gst_caps_set_value (intersection, "width", &g_width);
  gst_caps_set_value (intersection, "height", &g_height);

  /* Check if fixed caps supports DMA buffer */
  if (gst_qticodec2vdec_caps_has_feature (intersection,
          GST_CAPS_FEATURE_MEMORY_DMABUF)) {
    dec->downstream_supports_dma = TRUE;
    GST_DEBUG_OBJECT (dec, "Downstream supports DMA buffer");
  }

  GST_INFO_OBJECT (dec, "DMA output feature is %s",
      (dec->downstream_supports_dma ? "enabled" : "disabled"));

  switch (dec->map_outbuf) {
    case 0:
      actual_map = FALSE;
      break;
    case 1:
      actual_map = TRUE;
      break;
    default:
      actual_map = (dec->downstream_supports_dma) ? FALSE : TRUE;
      break;
  }

  if (!c2component_mapOutBuffer (dec->comp, actual_map)) {

    GST_ERROR_OBJECT (dec, "Failed to set map config");
    goto error_setup_output;
  }

  dec->output_state->caps = intersection;
  GST_INFO_OBJECT (dec, "output caps: %" GST_PTR_FORMAT,
      dec->output_state->caps);

  dec->outPixelfmt = output_format;

  GST_LOG_OBJECT (dec, "output width: %d, height: %d, format: %d",
      dec->width, dec->height, output_format);

  if (config) {
    pixelformat =
        make_pixelFormat_param (gst_to_c2_pixelformat (decoder, output_format),
        FALSE);
    GST_LOG_OBJECT (dec, "set c2 output format: %d",
        pixelformat.pixelFormat.fmt);
    g_ptr_array_add (config, &pixelformat);
  } else {
    goto error_setup_output;
  }

  GST_DEBUG_OBJECT (dec, "Complete setup output");

done:
  return ret;

error_setup_output:
  return GST_FLOW_ERROR;
}

/* Called when the element starts processing. Opening external resources. */
static gboolean
gst_qticodec2vdec_start (GstVideoDecoder * decoder)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);

  GST_DEBUG_OBJECT (dec, "start");

  return TRUE;
}

/* Called when the element stops processing. Close external resources. */
static gboolean
gst_qticodec2vdec_stop (GstVideoDecoder * decoder)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);

  GST_DEBUG_OBJECT (dec, "stop");

  return TRUE;
}

/* Dispatch any pending remaining data at EOS. Class can refuse to decode new data after. */
static GstFlowReturn
gst_qticodec2vdec_finish (GstVideoDecoder * decoder)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);
  gint64 timeout;
  BufferDescriptor inBuf;

  GST_DEBUG_OBJECT (dec, "finish");

  memset (&inBuf, 0, sizeof (BufferDescriptor));
  inBuf.fd = -1;
  inBuf.data = NULL;
  inBuf.size = 0;
  inBuf.timestamp = 0;
  inBuf.index = dec->frame_index;
  inBuf.flag = FLAG_TYPE_END_OF_STREAM;
  inBuf.pool_type = BUFFER_POOL_BASIC_LINEAR;

  /* Setup EOS work */
  if (dec->comp) {
    /* Queue buffer to Codec2 */
    c2component_queue (dec->comp, &inBuf);
  }

  /* wait for all the pending buffers to return */
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
  g_mutex_lock (&dec->pending_lock);
  if (!dec->eos_reached) {
    GST_DEBUG_OBJECT (dec, "wait until EOS signal is triggered");

    timeout =
        g_get_monotonic_time () + (EOS_WAITING_TIMEOUT * G_TIME_SPAN_SECOND);
    if (!g_cond_wait_until (&dec->pending_cond, &dec->pending_lock, timeout)) {
      GST_ERROR_OBJECT (dec, "Timed out on wait, exiting!");
    }
  } else {
    GST_DEBUG_OBJECT (dec, "EOS reached on output, finish the decoding");
  }

  g_mutex_unlock (&dec->pending_lock);
  GST_VIDEO_DECODER_STREAM_LOCK (decoder);

  return GST_FLOW_OK;
}

/* Called to inform the caps describing input video data that decoder is about to receive.
  Might be called more than once, if changing input parameters require reconfiguration.*/
static gboolean
gst_qticodec2vdec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);
  GstStructure *structure;
  const gchar *mode;
  gint retval = 0;
  gboolean ret = FALSE;
  gint width = 0;
  gint height = 0;
  GstVideoInterlaceMode interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
  INTERLACE_MODE_TYPE c2interlace_mode = INTERLACE_MODE_PROGRESSIVE;
  gchar *streamformat;
  GPtrArray *config = NULL;
  ConfigParams resolution;
  ConfigParams interlace;
  ConfigParams output_picture_order_mode;
  ConfigParams low_latency_mode;

  GST_DEBUG_OBJECT (dec, "set_format");

  structure = gst_caps_get_structure (state->caps, 0);
  streamformat =
      gst_to_c2_streamformat (decoder, structure, dec->low_latency_mode);
  if (!streamformat) {
    goto error_format;
  }

  retval = gst_structure_get_int (structure, "width", &width);
  retval &= gst_structure_get_int (structure, "height", &height);
  if (!retval) {
    goto error_res;
  }

  if (dec->input_setup) {
    /* Don't handle input format change here */
    goto done;
  }

  if ((mode = gst_structure_get_string (structure, "interlace-mode"))) {
    if (g_str_equal ("progressive", mode)) {
      interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
      c2interlace_mode = INTERLACE_MODE_PROGRESSIVE;
    } else if (g_str_equal ("interleaved", mode)) {
      interlace_mode = GST_VIDEO_INTERLACE_MODE_INTERLEAVED;
      c2interlace_mode = INTERLACE_MODE_INTERLEAVED_TOP_FIRST;
    } else if (g_str_equal ("mixed", mode)) {
      interlace_mode = GST_VIDEO_INTERLACE_MODE_MIXED;
      c2interlace_mode = INTERLACE_MODE_INTERLEAVED_TOP_FIRST;
    } else if (g_str_equal ("fields", mode)) {
      interlace_mode = GST_VIDEO_INTERLACE_MODE_FIELDS;
      c2interlace_mode = INTERLACE_MODE_FIELD_TOP_FIRST;
    }
  }

  dec->width = width;
  dec->height = height;
  dec->interlace_mode = interlace_mode;
  dec->streamformat = streamformat;

  if (dec->input_state) {
    gst_video_codec_state_unref (dec->input_state);
  }

  dec->input_state = gst_video_codec_state_ref (state);

  if (!gst_qticodec2vdec_create_component (decoder)) {
    goto error_set_format;
  }

  config = g_ptr_array_new ();

  resolution = make_resolution_param (width, height, TRUE);
  g_ptr_array_add (config, &resolution);

  interlace = make_interlace_param (c2interlace_mode, FALSE);
  g_ptr_array_add (config, &interlace);

  if (dec->output_picture_order_mode !=
      GST_QTI_CODEC2_DEC_OUTPUT_PICTURE_ORDER_MODE_DEFAULT) {
    output_picture_order_mode =
        make_output_picture_order_param (dec->output_picture_order_mode);
    g_ptr_array_add (config, &output_picture_order_mode);
  }

  if (dec->low_latency_mode) {
    low_latency_mode = make_low_latency_param (dec->low_latency_mode);
    g_ptr_array_add (config, &low_latency_mode);
  }

  /* Negotiate with downstream and setup output */
  if (GST_FLOW_OK != gst_qticodec2vdec_setup_output (decoder, config)) {
    g_ptr_array_free (config, FALSE);
    goto error_set_format;
  }

  if (!c2componentInterface_config (dec->comp_intf,
          config, BLOCK_MODE_MAY_BLOCK)) {
    GST_WARNING_OBJECT (dec, "Failed to set config");
  }

  g_ptr_array_free (config, FALSE);

  /* Start decoder */
  if (!c2component_start (dec->comp)) {
    GST_ERROR_OBJECT (dec, "Failed to start component");
    goto error_set_format;
  }

  ret = c2component_createBlockpool (dec->comp, BUFFER_POOL_BASIC_GRAPHIC);
  if (ret == FALSE) {
    GST_ERROR_OBJECT (dec, "Failed to create graphic pool");
    return FALSE;
  }
  /* let C2 component use graphic block pool created by client */
  ret = c2component_configBlockpool (dec->comp, BUFFER_POOL_BASIC_GRAPHIC);
  if (ret == FALSE) {
    GST_ERROR_OBJECT (dec,
        "Failed to let component use graphic pool created by client");
    return FALSE;
  }

done:
  dec->input_setup = TRUE;
  return TRUE;

  /* Errors */
error_format:
  {
    GST_ERROR_OBJECT (dec, "Unsupported format in caps: %" GST_PTR_FORMAT,
        state->caps);
    return FALSE;
  }
error_res:
  {
    GST_ERROR_OBJECT (dec, "Unable to get width/height value");
    return FALSE;
  }
error_set_format:
  {
    GST_ERROR_OBJECT (dec, "failed to setup input");
    return FALSE;
  }

  return TRUE;
}

/* Called when the element changes to GST_STATE_READY */
static gboolean
gst_qticodec2vdec_open (GstVideoDecoder * decoder)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (dec, "open");

  /* Create component store */
  dec->comp_store = c2componentStore_create ();

  return ret;
}

/* Called when the element changes to GST_STATE_NULL */
static gboolean
gst_qticodec2vdec_close (GstVideoDecoder * decoder)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (dec, "close");

  /* Stop the component */
  if (dec->comp) {
    ret = c2component_stop (dec->comp);
  }

  if (dec->input_state) {
    gst_video_codec_state_unref (dec->input_state);
    dec->input_state = NULL;
  }

  if (dec->output_state) {
    gst_video_codec_state_unref (dec->output_state);
    dec->output_state = NULL;
  }

  return TRUE;
}

/* Called whenever a input frame from the upstream is sent to decoder */
static GstFlowReturn
gst_qticodec2vdec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT (dec, "handle_frame");

  if (!dec->input_setup) {
    return GST_FLOW_OK;
  }

  GST_DEBUG_OBJECT (dec,
      "Frame number : %d, Distance from Sync : %d, Presentation timestamp : %"
      GST_TIME_FORMAT, frame->system_frame_number, frame->distance_from_sync,
      GST_TIME_ARGS (frame->pts));

  /* Decode frame */
  if (frame) {
    return gst_qticodec2vdec_decode (decoder, frame);
  } else {
    GST_DEBUG_OBJECT (dec, "EOS reached in handle_frame");
    return GST_FLOW_EOS;
  }
}

static gboolean
gst_qticodec2vdec_src_query (GstVideoDecoder * decoder, GstQuery * query)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);
  GstPad *pad = GST_VIDEO_DECODER_SRC_PAD (decoder);
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (dec, "src_query of type '%s'",
      gst_query_type_get_name (GST_QUERY_TYPE (query)));

  switch (GST_QUERY_TYPE (query)) {
    default:
      ret = GST_VIDEO_DECODER_CLASS (parent_class)->src_query (decoder, query);
      break;
  }

  return ret;
}

static gboolean
gst_qticodec2vdec_sink_query (GstVideoDecoder * decoder, GstQuery * query)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);
  GstPad *pad = GST_VIDEO_DECODER_SINK_PAD (decoder);
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (dec, "sink_query of type '%s'",
      gst_query_type_get_name (GST_QUERY_TYPE (query)));

  switch (GST_QUERY_TYPE (query)) {
    default:
      ret = GST_VIDEO_DECODER_CLASS (parent_class)->sink_query (decoder, query);
      break;
  }

  return ret;
}

static gboolean
gst_qticodec2vdec_caps_has_feature (const GstCaps * caps, const gchar * partten)
{
  guint count = gst_caps_get_size (caps);
  gboolean ret = FALSE;

  if (count > 0) {
    for (gint i = 0; i < count; i++) {
      GstCapsFeatures *features = gst_caps_get_features (caps, i);
      if (gst_caps_features_is_any (features))
        continue;
      if (gst_caps_features_contains (features, partten)) {
        ret = TRUE;
        break;
      }
    }
  }

  return ret;
}

static void
destroy_gst_buffer (gpointer data)
{
  GstBuffer *gst_buf = (GstBuffer *) data;
  if (gst_buf) {
    GST_DEBUG ("destory gst buffer:%p ref_cnt:%d", gst_buf,
        GST_OBJECT_REFCOUNT (gst_buf));
    gst_buffer_unref (gst_buf);
  }
}

static gboolean
gst_qticodec2vdec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query)
{
  GstCaps *outcaps;
  GstStructure *config;
  guint size, min, max;
  gboolean update = FALSE;
  gboolean use_peer_pool = FALSE;
  guint method, flag;
  GstVideoFormat input_format, output_format;
  gint input_width, input_height, output_width, output_height;
  GHashTable *buffer_table = NULL;
  GstBufferPool *out_port_pool = NULL;

  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);

  GST_DEBUG_OBJECT (dec, "decide allocation");

  out_port_pool = dec->out_port_pool;

  GstAllocationParams params = { (GstMemoryFlags) 0 };
  GstBufferPool *pool = NULL;
  GstAllocator *allocator = NULL;
  min = max = size = 0;

  gst_query_parse_allocation (query, &outcaps, NULL);

  GST_DEBUG_OBJECT (dec, "allocation caps: %" GST_PTR_FORMAT, outcaps);
  GST_DEBUG_OBJECT (dec, "allocation params: %" GST_PTR_FORMAT, query);

  if (gst_query_get_n_allocation_params (query) > 0)
    gst_query_parse_nth_allocation_param (query, 0, NULL, &params);

  /* Since decoder's output buffer can't be allocated out of C2 framework.
   * Therefore, can't use buffer pool from downstream. */
  if (gst_query_get_n_allocation_pools (query) > 0) {
    update = TRUE;
    gst_query_parse_nth_allocation_pool (query, 0, &pool, NULL, &min, &max);
    if (pool) {
      GST_DEBUG_OBJECT (dec, "discard buffer pool from downstream");
      gst_object_unref (pool);
      pool = NULL;
    }
  }

  if (gst_qticodec2vdec_caps_has_feature (outcaps,
          GST_CAPS_FEATURE_MEMORY_DMABUF)) {
    GST_INFO_OBJECT (dec, "downstream support dma buffer");
  } else {
    GST_INFO_OBJECT (dec,
        "downstream don't support dma buffer, return directly");
    return FALSE;
  }

  if (!use_peer_pool) {
    if (out_port_pool) {
      gst_object_unref (out_port_pool);
    }

    allocator = gst_dmabuf_allocator_new ();
    buffer_table =
        g_hash_table_new_full (g_int64_hash, g_int64_equal, g_free,
        destroy_gst_buffer);
    pool = gst_qticodec2vdec_buffer_pool_new (dec, allocator, buffer_table);

    if (max)
      max = MAX (MAX (min, max), QCODEC2_MIN_OUTBUFFERS);

    min = MAX (min, QCODEC2_MIN_OUTBUFFERS);
    /* disable gst buffer pool's allocator, since actual buffer(underlying dma/ion buffer)
     * is allocated inside of C2 allocator */
    size = 0;

    config = gst_buffer_pool_get_config (pool);

    GST_DEBUG_OBJECT (dec, "allocation: size:%u min:%u max:%u pool:%"
        GST_PTR_FORMAT, size, min, max, pool);

    gst_buffer_pool_config_set_params (config, outcaps, size, min, max);

    GST_DEBUG_OBJECT (dec, "setting own pool config to %"
        GST_PTR_FORMAT, config);

    /* configure own pool */
    if (!gst_buffer_pool_set_config (pool, config)) {
      GST_ERROR_OBJECT (dec, "configure our own buffer pool failed");
      goto cleanup;
    }

    /* For simplicity, simply read back the active configuration, so our base
     * class get the right information */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, NULL, &size, &min, &max);
    gst_structure_free (config);
  }

  GST_DEBUG_OBJECT (dec, "setting pool with size: %d, min: %d, max: %d",
      size, min, max);

  /* update pool info in the query */
  if (update) {
    GST_DEBUG_OBJECT (dec, "update buffer pool");
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  } else {
    GST_DEBUG_OBJECT (dec, "new buffer pool");
    gst_query_add_allocation_pool (query, pool, size, min, max);
  }

  dec->out_port_pool = pool;

  return TRUE;

cleanup:
  {
    if (pool) {
      gst_object_unref (pool);
    }
    return FALSE;
  }

}

static GstBuffer *
gst_qticodec2vdec_wrap_output_buffer (GstVideoDecoder * decoder,
    BufferDescriptor * decode_buf)
{
  GstBuffer *out_buf;
  GstVideoCodecState *state;
  GstVideoInfo *vinfo;
  GstStructure *structure = NULL;
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);
  guint output_size = decode_buf->size;
  GstBufferPoolAcquireParamsExt param_ext;
  guint64 *p_modifier = NULL;

  memset (&param_ext, 0, sizeof (GstBufferPoolAcquireParamsExt));

  state = gst_video_decoder_get_output_state (decoder);
  if (state) {
    vinfo = &state->info;
  } else {
    GST_ERROR_OBJECT (dec, "Failed to get decoder output state");
    return NULL;
  }

  if (!dec->downstream_supports_dma) {
    if (decode_buf->data) {
      GST_DEBUG_OBJECT (dec, "wrap buffer: %p, size: %d", decode_buf->data,
          output_size);
      out_buf =
          gst_buffer_new_wrapped_full (0, (gpointer) decode_buf->data,
          output_size, 0, output_size, NULL, NULL);
    }
  } else {
    param_ext.fd = decode_buf->fd;
    param_ext.meta_fd = decode_buf->meta_fd;
    param_ext.index = decode_buf->index;
    param_ext.size = decode_buf->size;
    gst_buffer_pool_acquire_buffer (dec->out_port_pool, &out_buf,
        (GstBufferPoolAcquireParams *) & param_ext);
  }

  if (out_buf) {
    if (!dec->downstream_supports_dma) {
      GstVideoMeta *QGVMeta = NULL;

      QGVMeta = gst_buffer_get_video_meta (out_buf);
      if (!QGVMeta) {
        QGVMeta =
            gst_buffer_add_video_meta_full (out_buf, GST_VIDEO_FRAME_FLAG_NONE,
            GST_VIDEO_INFO_FORMAT (vinfo), GST_VIDEO_INFO_WIDTH (vinfo),
            GST_VIDEO_INFO_HEIGHT (vinfo), GST_VIDEO_INFO_N_PLANES (vinfo),
            vinfo->offset, vinfo->stride);
        if (!QGVMeta) {
          GST_ERROR_OBJECT (dec, "Failed to attach video info into meta");
          goto fail;
        }
      }
      GST_DEBUG_OBJECT (dec, "offset:%ld %ld stride:%d %d", vinfo->offset[0],
          vinfo->offset[1], vinfo->stride[0], vinfo->stride[1]);

      QGVMeta->offset[2] = GST_MAKE_FOURCC ('Q', 'a', 'U', 'T');
      QGVMeta->offset[3] = output_size;
      QGVMeta->stride[2] = decode_buf->fd;
      QGVMeta->stride[3] = decode_buf->meta_fd;
    } else {
      gst_mini_object_set_qdata (GST_MINI_OBJECT (out_buf),
          qticodec2_c2buf_qdata_quark (), decode_buf->c2_buffer, NULL);
    }
  } else {
    GST_ERROR_OBJECT (dec, "Fail to allocate output gst buffer");
    goto fail;
  }

  if (decode_buf->gbm_bo) {
    /* That gstreamer buf is probably already attached modifier, check it at first.
     * As modifier only store some usage info. like ubwc and security, common event
     * like resolution change won't change modifier. Therefore, if already attached
     * modifier, needn't update or re-attach it. */
    if (!gst_mini_object_get_qdata (GST_MINI_OBJECT_CAST (out_buf),
            gst_fbuf_modifier_qdata_quark ())) {
      p_modifier = g_slice_new (guint64);
      if (!dec->gbm_api_bo_get_modifier) {
        *p_modifier = DRM_FORMAT_MOD_INVALID;
      } else {
        *p_modifier = dec->gbm_api_bo_get_modifier (decode_buf->gbm_bo);
      }
      gst_mini_object_set_qdata (GST_MINI_OBJECT (out_buf),
          gst_fbuf_modifier_qdata_quark (), p_modifier,
          (GDestroyNotify) modifier_free);
      GST_DEBUG_OBJECT (dec,
          "Attach modifier quark %p, value:0x%lx on gstbuf %p", p_modifier,
          *p_modifier, out_buf);
    }
  }

  if (!dec->downstream_supports_dma) {
    /* Creates a new, empty GstStructure with the given name */
    structure = gst_structure_new_empty ("BUFFER");
    gst_structure_set (structure,
        "decoder", G_TYPE_POINTER, decoder,
        "index", G_TYPE_UINT64, decode_buf->index, NULL);

    /* Set a notification function to signal when the buffer is no longer used. */
    gst_mini_object_set_qdata (GST_MINI_OBJECT (out_buf),
        qticodec2vdec_qdata_quark (), structure,
        (GDestroyNotify) gst_video_decoder_buffer_release);
  }
done:
  gst_video_codec_state_unref (state);
  return out_buf;

fail:
  if (out_buf) {
    gst_buffer_unref (out_buf);
    out_buf = NULL;
  }
  goto done;
}

static void
gst_video_decoder_buffer_release (GstStructure * structure)
{
  GstVideoDecoder *decoder = NULL;
  Gstqticodec2vdec *dec = NULL;
  guint64 index = 0;

  gst_structure_get (structure, "decoder", G_TYPE_POINTER, &decoder, NULL);
  gst_structure_get_uint64 (structure, "index", &index);

  if (decoder) {
    dec = GST_QTICODEC2VDEC (decoder);

    GST_DEBUG_OBJECT (dec, "release output buffer index: %ld", index);
    if (!c2component_freeOutBuffer (dec->comp, index)) {
      GST_ERROR_OBJECT (dec, "Failed to release the buffer (%lu)", index);
    }
  } else {
    GST_ERROR_OBJECT (dec, "Null Gstqticodec2vdec hanlde");
  }

  gst_structure_free (structure);
}

/* Push decoded frame to downstream element */
static GstFlowReturn
push_frame_downstream (GstVideoDecoder * decoder, BufferDescriptor * decode_buf)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);
  GstBuffer *outbuf;
  GstVideoCodecFrame *frame;
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoCodecState *state;
  GstVideoInfo *vinfo;

  GST_DEBUG_OBJECT (dec, "push_frame_downstream");

  state = gst_video_decoder_get_output_state (decoder);
  if (state) {
    vinfo = &state->info;
  } else {
    GST_ERROR_OBJECT (dec, "video codec state is NULL, unexpected!");
    goto out;
  }

  GST_DEBUG_OBJECT (dec,
      "push_frame_downstream, buffer: %p, fd: %d, meta_fd: %d, timestamp: %lu",
      decode_buf->data, decode_buf->fd, decode_buf->meta_fd,
      decode_buf->timestamp);

  frame = gst_video_decoder_get_frame (decoder, decode_buf->index);
  if (frame == NULL) {
    GST_ERROR_OBJECT (dec,
        "Error in gst_video_decoder_get_frame, frame number: %lu",
        decode_buf->index);
    goto out;
  }

  guint output_size = decode_buf->size;
  outbuf = gst_qticodec2vdec_wrap_output_buffer (decoder, decode_buf);
  if (outbuf) {
    gst_buffer_set_flags (outbuf, GST_BUFFER_FLAG_SYNC_AFTER);
    GST_BUFFER_PTS (outbuf) =
        gst_util_uint64_scale (decode_buf->timestamp, GST_SECOND,
        C2_TICKS_PER_SECOND);

    if (state->info.fps_d != 0 && state->info.fps_n != 0) {
      GST_BUFFER_DURATION (outbuf) = gst_util_uint64_scale (GST_SECOND,
          vinfo->fps_d, vinfo->fps_n);
    }
    frame->output_buffer = outbuf;

    GST_DEBUG_OBJECT (dec,
        "out buffer: PTS: %lu, duration: %lu, fps_d: %d, fps_n: %d",
        GST_BUFFER_PTS (outbuf), GST_BUFFER_DURATION (outbuf), vinfo->fps_d,
        vinfo->fps_n);
  }

  /* Decrease the refcount of the frame so that the frame is released by the
   * gst_video_decoder_finish_frame function and so that the output buffer is
   * writable when it's pushed downstream */
  gst_video_codec_frame_unref (frame);
  ret = gst_video_decoder_finish_frame (decoder, frame);
  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (dec, "Failed(%d) to push frame downstream", ret);
    goto out;
  }

  gst_video_codec_state_unref (state);
  return GST_FLOW_OK;

out:
  gst_video_codec_state_unref (state);
  return GST_FLOW_ERROR;
}

/* Handle event from Codec2 */
static void
handle_video_event (const void *handle, EVENT_TYPE type, void *data)
{

  GstVideoDecoder *decoder = (GstVideoDecoder *) handle;
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (dec, "handle_video_event");

  switch (type) {
    case EVENT_OUTPUTS_DONE:{
      BufferDescriptor *outBuffer = (BufferDescriptor *) data;
      if (!(outBuffer->flag & FLAG_TYPE_END_OF_STREAM)) {
        if (!dec->output_setup || dec->width != outBuffer->width
            || dec->height != outBuffer->height) {
          if (dec->output_setup) {
            GST_DEBUG_OBJECT (dec,
                "resolution change, width height:%d %d -> %u %u", dec->width,
                dec->height, outBuffer->width, outBuffer->height);
          }

          dec->width = outBuffer->width;
          dec->height = outBuffer->height;
          GstCaps *new_caps = gst_caps_copy (dec->output_state->caps);
          dec->output_state =
              gst_video_decoder_set_output_state (decoder, dec->outPixelfmt,
              dec->width, dec->height, dec->input_state);

          GValue new_width = { 0, };
          GValue new_height = { 0, };
          g_value_init (&new_width, G_TYPE_INT);
          g_value_set_int (&new_width, dec->width);

          g_value_init (&new_height, G_TYPE_INT);
          g_value_set_int (&new_height, dec->height);

          gst_caps_set_value (new_caps, "width", &new_width);
          gst_caps_set_value (new_caps, "height", &new_height);
          dec->output_state->caps = new_caps;

          if (!gst_video_decoder_negotiate (decoder)) {
            gst_video_codec_state_unref (dec->output_state);
            GST_ERROR_OBJECT (dec, "Failed to negotiate");
            break;
          }
          gst_pad_check_reconfigure (decoder->srcpad);

          dec->output_setup = TRUE;
        }
      }
      if (outBuffer->size) {
        if (!dec->first_frame_time.tv_sec && !dec->first_frame_time.tv_usec) {
          gettimeofday (&dec->first_frame_time, NULL);
          int time_1st_cost_us =
              (dec->first_frame_time.tv_sec -
              dec->start_time.tv_sec) * 1000000 +
              (dec->first_frame_time.tv_usec - dec->start_time.tv_usec);
          GST_DEBUG_OBJECT (dec, "first frame latency:%d us", time_1st_cost_us);
        }
        dec->num_output_done++;
        GST_DEBUG_OBJECT (dec, "output done, count: %lu", dec->num_output_done);
        ret = push_frame_downstream (decoder, outBuffer);
        if (ret != GST_FLOW_OK) {
          GST_ERROR_OBJECT (dec, "Failed to push frame downstream");
        }
      } else if (outBuffer->flag & FLAG_TYPE_END_OF_STREAM) {
        GST_INFO_OBJECT (dec, "Decoder reached EOS");
        g_mutex_lock (&dec->pending_lock);
        dec->eos_reached = TRUE;
        g_cond_signal (&dec->pending_cond);
        g_mutex_unlock (&dec->pending_lock);
      }
      break;
    }
    case EVENT_TRIPPED:{
      GST_ERROR_OBJECT (dec, "Failed to apply configuration setting(%d)",
          *(gint32 *) data);
      break;
    }
    case EVENT_ERROR:{
      GST_ERROR_OBJECT (dec, "Something un-expected happened(%d)",
          *(gint32 *) data);
      break;
    }
    default:{
      GST_ERROR_OBJECT (dec, "Invalid Event(%d)", type);
      break;
    }
  }
}

/* Push frame to Codec2 */
static GstFlowReturn
gst_qticodec2vdec_decode (GstVideoDecoder * decoder, GstVideoCodecFrame * frame)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (decoder);
  GstMapInfo mapinfo = { 0, };
  GstBuffer *buf = NULL;
  BufferDescriptor inBuf;
  gboolean status = FALSE;
  GstFlowReturn ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT (dec, "decode");
  if (!frame) {
    GST_WARNING_OBJECT (dec, "frame is NULL, ret GST_FLOW_EOS");
    return GST_FLOW_EOS;
  }

  memset (&inBuf, 0, sizeof (BufferDescriptor));

  buf = frame->input_buffer;
  gst_buffer_map (buf, &mapinfo, GST_MAP_READ);
  inBuf.fd = -1;
  inBuf.data = mapinfo.data;
  inBuf.size = mapinfo.size;
  inBuf.pool_type = BUFFER_POOL_BASIC_LINEAR;

  GST_INFO_OBJECT (dec, "frame->pts (%" G_GUINT64_FORMAT ")", frame->pts);

  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  /* Keep track of queued frame */
  dec->queued_frame[(dec->frame_index) % MAX_QUEUED_FRAME] =
      frame->system_frame_number;

  inBuf.timestamp = NANO_TO_MILLI (frame->pts);
  inBuf.index = frame->system_frame_number;

  /* Queue buffer to Codec2 */
  status = c2component_queue (dec->comp, &inBuf);
  gst_buffer_unmap (buf, &mapinfo);
  if (!status) {
    GST_ERROR_OBJECT(dec, "failed to queue input frame to Codec2");
    ret = GST_FLOW_ERROR;
    goto out;
  }

  g_mutex_lock (&(dec->pending_lock));
  dec->frame_index += 1;
  dec->num_input_queued++;
  g_mutex_unlock (&(dec->pending_lock));

out:
  GST_VIDEO_DECODER_STREAM_LOCK (decoder);

  return ret;
}

static void
gst_qticodec2vdec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (object);

  GST_DEBUG_OBJECT (dec, "qticodec2vdec_set_property");

  switch (prop_id) {
    case PROP_SILENT:
      dec->silent = g_value_get_boolean (value);
      break;
    case PROP_OUTPUT_PICTURE_ORDER:
      dec->output_picture_order_mode = g_value_get_uint (value);
      break;
    case PROP_LOW_LATENCY:
      dec->low_latency_mode = g_value_get_boolean (value);
      break;
    case PROP_MAP_OUTBUF:
      dec->map_outbuf = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_qticodec2vdec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (object);

  GST_DEBUG_OBJECT (dec, "qticodec2vdec_get_property");

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, dec->silent);
      break;
    case PROP_OUTPUT_PICTURE_ORDER:
      g_value_set_uint (value, dec->output_picture_order_mode);
      break;
    case PROP_LOW_LATENCY:
      g_value_set_boolean (value, dec->low_latency_mode);
      break;
    case PROP_MAP_OUTBUF:
      g_value_set_uint (value, dec->map_outbuf);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Called during object destruction process */
static void
gst_qticodec2vdec_finalize (GObject * object)
{
  Gstqticodec2vdec *dec = GST_QTICODEC2VDEC (object);

  GST_DEBUG_OBJECT (dec, "finalize");

  g_mutex_clear (&dec->pending_lock);
  g_cond_clear (&dec->pending_cond);

  if (dec->streamformat) {
    g_free (dec->streamformat);
    dec->streamformat = NULL;
  }

  if (dec->out_port_pool) {
    GST_DEBUG_OBJECT (dec, "pool ref cnt:%d",
        GST_OBJECT_REFCOUNT (dec->out_port_pool));
    gst_object_unref (dec->out_port_pool);
  }

  if (!gst_qticodec2vdec_destroy_component (GST_VIDEO_DECODER (object))) {
    GST_ERROR_OBJECT (dec, "Failed to delete component");
  }

  if (dec->comp_store) {
    c2componentStore_delete (dec->comp_store);
  }

  if (dec->gbm_lib) {
    GST_INFO_OBJECT (dec, "dlclose gbm lib:%p", dec->gbm_lib);
    dlclose (dec->gbm_lib);
  }

  /* Lastly chain up to the parent class */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* entry point to initialize the plug-in
 * register the plugin
 */
static gboolean
plugin_init (GstPlugin * qticodec2vdec)
{
  /* debug category for fltering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_qticodec2vdec_debug, "qticodec2vdec",
      0, "QTI GST codec2.0 video decoder");

  return gst_element_register (qticodec2vdec, "qticodec2vdec",
      GST_RANK_PRIMARY + 10, GST_TYPE_QTICODEC2VDEC);
}

/* Initialize the qticodec2vdec's class */
static void
gst_qticodec2vdec_class_init (Gstqticodec2vdecClass * klass)
{
  GstVideoDecoderClass *video_decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_qtivdec_src_template));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_qtivdec_sink_template));

  /* Set GObject class property */
  gobject_class->set_property = gst_qticodec2vdec_set_property;
  gobject_class->get_property = gst_qticodec2vdec_get_property;
  gobject_class->finalize = gst_qticodec2vdec_finalize;

  /* Add property to this class */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_OUTPUT_PICTURE_ORDER, g_param_spec_uint ("output-picture-order-mode",
          "output picture order mode",
          "output picture order (0xffffffff=component default, 1: display order, 2: decoder order)",
          0, G_MAXUINT, GST_QTI_CODEC2_DEC_OUTPUT_PICTURE_ORDER_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_LOW_LATENCY,
      g_param_spec_boolean ("low-latency-mode", "Low latency mode",
          "If enabled, decoder should be in low latency mode",
          GST_QTI_CODEC2_DEC_LOW_LATENCY_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_MAP_OUTBUF,
      g_param_spec_uint ("map-outbuf", "Map output buffer",
          "enable output buffer mapping (0xffffffff=default, 0: always NOT map output buffer, 1: always map output buffer",
          0, G_MAXUINT, GST_QTI_CODEC2_DEC_MAP_OUTBUF_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  video_decoder_class->start = GST_DEBUG_FUNCPTR (gst_qticodec2vdec_start);
  video_decoder_class->stop = GST_DEBUG_FUNCPTR (gst_qticodec2vdec_stop);
  video_decoder_class->set_format =
      GST_DEBUG_FUNCPTR (gst_qticodec2vdec_set_format);
  video_decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_qticodec2vdec_handle_frame);
  video_decoder_class->finish = GST_DEBUG_FUNCPTR (gst_qticodec2vdec_finish);
  video_decoder_class->open = GST_DEBUG_FUNCPTR (gst_qticodec2vdec_open);
  video_decoder_class->close = GST_DEBUG_FUNCPTR (gst_qticodec2vdec_close);
  video_decoder_class->src_query =
      GST_DEBUG_FUNCPTR (gst_qticodec2vdec_src_query);
  video_decoder_class->sink_query =
      GST_DEBUG_FUNCPTR (gst_qticodec2vdec_sink_query);
  video_decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_qticodec2vdec_decide_allocation);
  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Codec2 video decoder", "Decoder/Video",
      "Video Decoder based on Codec2.0", "QTI");
}

/* Invoked during object instantiation (equivalent C++ constructor). */
static void
gst_qticodec2vdec_init (Gstqticodec2vdec * dec)
{
  GstVideoDecoder *decoder = (GstVideoDecoder *) dec;

  gst_video_decoder_set_packetized (decoder, TRUE);

  dec->input_setup = FALSE;
  dec->output_setup = FALSE;
  dec->eos_reached = FALSE;
  dec->frame_index = 0;
  dec->num_input_queued = 0;
  dec->num_output_done = 0;
  dec->downstream_supports_dma = FALSE;
  dec->comp_store = NULL;
  dec->comp = NULL;
  dec->comp_intf = NULL;
  dec->output_picture_order_mode =
      GST_QTI_CODEC2_DEC_OUTPUT_PICTURE_ORDER_MODE_DEFAULT;
  dec->low_latency_mode = GST_QTI_CODEC2_DEC_LOW_LATENCY_MODE_DEFAULT;
  dec->map_outbuf = GST_QTI_CODEC2_DEC_MAP_OUTBUF_DEFAULT;
  dec->out_port_pool = NULL;

  memset (dec->queued_frame, 0, MAX_QUEUED_FRAME);
  memset (&dec->start_time, 0, sizeof (struct timeval));
  memset (&dec->first_frame_time, 0, sizeof (struct timeval));
  gettimeofday (&dec->start_time, NULL);

  g_cond_init (&dec->pending_cond);
  g_mutex_init (&dec->pending_lock);

  dec->silent = FALSE;
  dec->gbm_lib = dlopen ("libgbm.so", RTLD_NOW);
  GST_INFO_OBJECT (dec, "open gbm lib:%p", dec->gbm_lib);
  if (dec->gbm_lib == NULL) {
    GST_ERROR ("dlopen libgbm.so failed");
    return;
  }

  dec->gbm_api_bo_get_modifier = dlsym (dec->gbm_lib, "gbm_bo_get_modifier");
  if (!dec->gbm_api_bo_get_modifier) {
    GST_ERROR_OBJECT (dec, "Failed as a gbm API is null");
    dlclose (dec->gbm_lib);
    dec->gbm_lib = NULL;
    return;
  }

}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    qticodec2vdec,
    "QTI GST Codec2.0 Video Decoder",
    plugin_init, VERSION, GST_LICENSE_UNKNOWN, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
