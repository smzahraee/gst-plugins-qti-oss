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


GST_DEBUG_CATEGORY_STATIC (gst_qticodec2venc_debug);
#define GST_CAT_DEFAULT gst_qticodec2venc_debug
#define GST_QTI_CODEC2_ENC_COLOR_SPACE_CONVERSION             (FALSE)

/* class initialization */
G_DEFINE_TYPE (Gstqticodec2venc, gst_qticodec2venc, GST_TYPE_VIDEO_ENCODER);

#define GST_TYPE_CODEC2_ENC_MIRROR_TYPE (gst_qticodec2venc_mirror_get_type ())
#define GST_TYPE_CODEC2_ENC_RATE_CONTROL (gst_qticodec2venc_rate_control_get_type ())
#define GST_TYPE_CODEC2_ENC_COLOR_PRIMARIES (gst_qticodec2venc_color_primaries_get_type())
#define GST_TYPE_CODEC2_ENC_MATRIX_COEFFS (gst_qticodec2venc_matrix_coeffs_get_type())
#define GST_TYPE_CODEC2_ENC_TRANSFER_CHAR (gst_qticodec2venc_transfer_characteristics_get_type())
#define GST_TYPE_CODEC2_ENC_FULL_RANGE (gst_qticodec2venc_full_range_get_type())
#define GST_TYPE_CODEC2_ENC_INTRA_REFRESH_MODE (gst_qticodec2venc_intra_refresh_mode_get_type ())
#define GST_TYPE_CODEC2_ENC_SLICE_MODE (gst_qticodec2venc_slice_mode_get_type ())
#define parent_class gst_qticodec2venc_parent_class
#define NANO_TO_MILLI(x)  ((x) / 1000)
#define EOS_WAITING_TIMEOUT 5
#define MAX_INPUT_BUFFERS 32

/* Function will be named qticodec2venc_qdata_quark() */
static G_DEFINE_QUARK(QtiCodec2EncoderQuark, qticodec2venc_qdata);
static G_DEFINE_QUARK(QtiCodec2C2BufQuark, qticodec2_c2buf_qdata);

enum
{
  PROP_0,
  PROP_SILENT,
  PROP_RATE_CONTROL,
  PROP_DOWNSCALE_WIDTH,
  PROP_DOWNSCALE_HEIGHT,
  PROP_COLOR_SPACE_PRIMARIES,
  PROP_COLOR_SPACE_MATRIX_COEFFS,
  PROP_COLOR_SPACE_TRANSFER_CHAR,
  PROP_COLOR_SPACE_FULL_RANGE,
  PROP_COLOR_SPACE_CONVERSION,
  PROP_MIRROR,
  PROP_ROTATION,
  PROP_INTRA_REFRESH_MODE,
  PROP_INTRA_REFRESH_MBS,
  PROP_TARGET_BITRATE,
  PROP_SLICE_MODE,
  PROP_SLICE_SIZE,
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
static gboolean gst_qticodec2venc_propose_allocation(GstVideoEncoder * encoder, GstQuery * query);

static void gst_qticodec2venc_set_property (GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void gst_qticodec2venc_get_property (GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static void gst_qticodec2venc_finalize (GObject* object);

static gboolean gst_qticodec2venc_create_component(GstVideoEncoder* encoder);
static gboolean gst_qticodec2venc_destroy_component(GstVideoEncoder* encoder);
static void handle_video_event (const void* handle, EVENT_TYPE type, void* data);

static GstFlowReturn gst_qticodec2venc_encode (GstVideoEncoder* encoder, GstVideoCodecFrame* frame);
static GstFlowReturn gst_qticodec2venc_setup_output (GstVideoEncoder* encoder, GstVideoCodecState* state);
static void gst_qticodec2venc_buffer_release (GstStructure* structure);

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
        "video/x-heic,"
        "stream-format = (string) { byte-stream },"
        "alignment = (string) { au }"
      )
    );

static GstStaticPadTemplate gst_qtivenc_sink_template =
GST_STATIC_PAD_TEMPLATE (GST_VIDEO_ENCODER_SINK_NAME,
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        "video/x-raw(memory:DMABuf), "
        "format = (string) NV12, "
        "width  = (int) [ 32, 4096 ], "
        "height = (int) [ 32, 4096 ],"
        "framerate = " GST_VIDEO_FPS_RANGE ""
        ";"
        "video/x-raw(memory:DMABuf), "
        "format = (string) NV12_UBWC, "
        "width  = (int) [ 32, 4096 ], "
        "height = (int) [ 32, 4096 ],"
        "framerate = " GST_VIDEO_FPS_RANGE ""
        ";"
        "video/x-raw(memory:DMABuf), "
        "format = (string) P010_10LE, "
        "width  = (int) [ 128, 8192 ], "
        "height = (int) [ 128, 8192 ],"
        "framerate = " GST_VIDEO_FPS_RANGE ""
        ";"
        "video/x-raw(memory:DMABuf), "
        "format = (string) NV12_10LE32_UBWC, "
        "width  = (int) [ 128, 8192 ], "
        "height = (int) [ 128, 8192 ],"
        "framerate = " GST_VIDEO_FPS_RANGE ""
        ";"
        "video/x-raw, "
        "format = (string) NV12, "
        "width  = (int) [ 32, 4096 ], "
        "height = (int) [ 32, 4096 ],"
        "framerate = " GST_VIDEO_FPS_RANGE ""
        ";"
        "video/x-raw, "
        "format = (string) NV12_UBWC, "
        "width  = (int) [ 32, 4096 ], "
        "height = (int) [ 32, 4096 ],"
        "framerate = " GST_VIDEO_FPS_RANGE ""
        ";"
        "video/x-raw, "
        "format = (string) P010_10LE, "
        "width  = (int) [ 128, 8192 ], "
        "height = (int) [ 128, 8192 ],"
        "framerate = " GST_VIDEO_FPS_RANGE ""
        ";"
        "video/x-raw, "
        "format = (string) NV12_10LE32_UBWC, "
        "width  = (int) [ 128, 8192 ], "
        "height = (int) [ 128, 8192 ],"
        "framerate = " GST_VIDEO_FPS_RANGE ""
      )
    );

static ConfigParams
make_bitrate_param (guint32 bitrate, gboolean isInput) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_BITRATE;
  param.isInput = isInput;
  param.val.u32 = bitrate;

  return param;
}

static ConfigParams
make_resolution_param (guint32 width, guint32 height, gboolean isInput) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_RESOLUTION;
  param.isInput = isInput;
  param.resolution.width = width;
  param.resolution.height = height;

  return param;
}

static ConfigParams
make_pixelFormat_param (guint32 fmt, gboolean isInput) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_PIXELFORMAT;
  param.isInput = isInput;
  param.pixelFormat.fmt = fmt;

  return param;
}

static ConfigParams
make_interlace_param (INTERLACE_MODE_TYPE mode, gboolean isInput) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_INTERLACE;
  param.isInput = isInput;
  param.interlaceMode.type = mode;

  return param;
}

static ConfigParams
make_mirror_param (MIRROR_TYPE mirror, gboolean isInput) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_MIRROR;
  param.isInput = isInput;
  param.mirror.type = mirror;

  return param;
}

static ConfigParams
make_rotation_param (guint32 rotation, gboolean isInput) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_ROTATION;
  param.isInput = isInput;
  param.val.u32 = rotation;

  return param;
}

static ConfigParams
make_rateControl_param (RC_MODE_TYPE mode) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_RATECONTROL;
  param.rcMode.type = mode;

  return param;
}

static ConfigParams
make_downscale_param (guint32 width, guint32 height) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_DOWNSCALE;
  param.resolution.width = width;
  param.resolution.height = height;

  return param;
}

static ConfigParams
make_slicemode_param (guint32 size, SLICE_MODE mode) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_SLICE_MODE;
  param.val.u32 = size;
  param.SliceMode.type = mode;

  return param;
}

static ConfigParams
make_colorSpaceConv_param (gboolean csc) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_ENC_CSC;
  param.color_space_conversion = csc;

  return param;
}

static ConfigParams
make_colorAspects_param (COLOR_PRIMARIES primaries, TRANSFER_CHAR transfer_char,
    MATRIX matrix, FULL_RANGE full_range) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_COLOR_ASPECTS_INFO;
  param.colorAspects.primaries = primaries;
  param.colorAspects.transfer_char = transfer_char;
  param.colorAspects.matrix = matrix;
  param.colorAspects.full_range = full_range;

  return param;
}

static ConfigParams
make_intraRefresh_param (IR_MODE_TYPE mode, guint32 intra_refresh_mbs) {
  ConfigParams param;

  memset(&param, 0, sizeof(ConfigParams));

  param.config_name = CONFIG_FUNCTION_KEY_INTRAREFRESH;
  param.irMode.type = mode;
  param.irMode.intra_refresh_mbs = (float)intra_refresh_mbs;

  return param;
}

static gchar*
gst_to_c2_streamformat (GstStructure* structure) {
  gchar *ret = NULL;

  if (gst_structure_has_name (structure, "video/x-h264")) {
    ret = g_strdup("c2.qti.avc.encoder");
  }
  else if (gst_structure_has_name (structure, "video/x-h265")) {
    ret = g_strdup("c2.qti.hevc.encoder");
  }
  else if (gst_structure_has_name (structure, "video/x-heic")) {
    ret = g_strdup("c2.qti.heic.encoder");
  }

  return ret;
}

static guint32
gst_to_c2_pixelformat (GstVideoFormat format) {
  guint32 result = 0;

  switch(format) {
    case GST_VIDEO_FORMAT_NV12 :
      result = PIXEL_FORMAT_NV12_LINEAR;
      break;
    case GST_VIDEO_FORMAT_NV12_UBWC :
      result = PIXEL_FORMAT_NV12_UBWC;
      break;
    case GST_VIDEO_FORMAT_P010_10LE:
      result = PIXEL_FORMAT_P010;
      break;
    case GST_VIDEO_FORMAT_NV12_10LE32_UBWC:
      result = PIXEL_FORMAT_TP10_UBWC;
      break;
    default:
      break;
  }

  return result;
}

static GType
gst_qticodec2venc_mirror_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {MIRROR_NONE,       "Mirror None", "none"},
      {MIRROR_VERTICAL,   "Mirror Vertical", "vertical"},
      {MIRROR_HORIZONTAL, "Mirror Horizontal", "horizontal"},
      {MIRROR_BOTH,       "Mirror Both", "both"},
      {0,                 NULL, NULL}
    };

    qtype = g_enum_register_static ("GstCodec2VencMirror", values);
  }
  return qtype;
}

static GType
gst_qticodec2venc_slice_mode_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {SLICE_MODE_DISABLE,       "Slice Mode Disable", "disable"},
      {SLICE_MODE_MB,   "Slice Mode MB", "MB"},
      {SLICE_MODE_BYTES, "Slice Mode Bytes", "bytes"},
      {0,                 NULL, NULL}
    };

    qtype = g_enum_register_static ("GstCodec2VencSliceMode", values);
  }
  return qtype;
}

static GType
gst_qticodec2venc_rate_control_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {RC_OFF,     "Disable RC", "disable"},
      {RC_CONST,   "Constant", "constant"},
      {RC_CBR_VFR, "Constant bitrate, variable framerate", "CBR-VFR"},
      {RC_VBR_CFR, "Variable bitrate, constant framerate", "VBR-CFR"},
      {RC_VBR_VFR, "Variable bitrate, variable framerate", "VBR-VFR"},
      {RC_CQ,      "Constant quality", "CQ"},
      {0,          NULL, NULL}
    };

    qtype = g_enum_register_static ("GstCodec2VencRateControl", values);
  }
  return qtype;
}

static GType
gst_qticodec2venc_color_primaries_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {COLOR_PRIMARIES_UNSPECIFIED, "primaries are unspecified", "NONE"},
      {COLOR_PRIMARIES_BT709, "Rec.ITU-R BT.709-6 or equivalent","BT709"},
      {COLOR_PRIMARIES_BT470_M, "Rec.ITU-R BT.470-6 System M or equivalent", "BT470_M"},
      {COLOR_PRIMARIES_BT601_625, "Rec.ITU-R BT.601-6 625 or equivalent", "BT601_625"},
      {COLOR_PRIMARIES_BT601_525, "Rec.ITU-R BT.601-6 525 or equivalent", "BT601_525"},
      {COLOR_PRIMARIES_GENERIC_FILM, "Generic Film", "GENERIC_FILM"},
      {COLOR_PRIMARIES_BT2020, "Rec.ITU-R BT.2020 or equivalent", "BT2020"},
      {COLOR_PRIMARIES_RP431, "SMPTE RP 431-2 or equivalent", "RP431"},
      {COLOR_PRIMARIES_EG432, "SMPTE EG 432-1 or equivalent", "EG432"},
      {COLOR_PRIMARIES_EBU3213, "EBU Tech.3213-E or equivalent", "EBU3213"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstCodec2VencColorPrimaries", values);
  }
  return qtype;
}

static GType
gst_qticodec2venc_matrix_coeffs_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {COLOR_MATRIX_UNSPECIFIED, "Matrix coefficients are unspecified", "NONE"},
      {COLOR_MATRIX_BT709, "Rec.ITU-R BT.709-5 or equivalent", "BT709"},
      {COLOR_MATRIX_FCC47_73_682, "FCC Title 47 CFR 73.682 or equivalent (KR=0.30, KB=0.11)", "FCC47_73_682"},
      {COLOR_MATRIX_BT601, "FCC Title 47 CFR 73.682 or equivalent (KR=0.30, KB=0.11)", "BT601"},
      {COLOR_MATRIX_240M, "SMPTE 240M or equivalent", "240M"},
      {COLOR_MATRIX_BT2020, "Rec.ITU-R BT.2020 non-constant luminance", "BT2020"},
      {COLOR_MATRIX_BT2020_CONSTANT, "Rec.ITU-R BT.2020 constant luminance", "BT2020_CONSTANT"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstCodec2VencMatrixCoeffs", values);
  }
  return qtype;
}

static GType
gst_qticodec2venc_transfer_characteristics_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {COLOR_TRANSFER_UNSPECIFIED, "Transfer is unspecified", "NONE"},
      {COLOR_TRANSFER_LINEAR, "Linear transfer characteristics", "LINEAR"},
      {COLOR_TRANSFER_SRGB, "sRGB or equivalent", "SRGB"},
      {COLOR_TRANSFER_170M, "SMPTE 170M or equivalent (e.g. BT.601/709/2020)", "170M"},
      {COLOR_TRANSFER_GAMMA22, "Assumed display gamma 2.2", "GAMMA22"},
      {COLOR_TRANSFER_GAMMA28, "Assumed display gamma 2.8", "GAMMA28"},
      {COLOR_TRANSFER_ST2084, "SMPTE ST 2084 for 10/12/14/16 bit systems", "ST2084"},
      {COLOR_TRANSFER_HLG, "ARIB STD-B67 hybrid-log-gamma", "HLG"},
      {COLOR_TRANSFER_240M, "SMPTE 240M or equivalent", "240M"},
      {COLOR_TRANSFER_XVYCC, "IEC 61966-2-4 or equivalent", "XVYCC"},
      {COLOR_TRANSFER_BT1361, "Rec.ITU-R BT.1361 extended gamut", "BT1361"},
      {COLOR_TRANSFER_ST428, "SMPTE ST 428-1 or equivalent", "ST428"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstCodec2VencTransferChar", values);
  }
  return qtype;
}

static GType
gst_qticodec2venc_full_range_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {COLOR_RANGE_UNSPECIFIED, "Range is unspecified", "NONE"},
      {COLOR_RANGE_FULL, "Full range", "FULL"},
      {COLOR_RANGE_LIMITED, "Limited range", "LIMITED"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstCodec2VencFullRange", values);
  }
  return qtype;
}

static GType
gst_qticodec2venc_intra_refresh_mode_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {IR_NONE, "None", "none"},
      {IR_RANDOM, "Random", "random"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstCodec2VencIntraRefreshMode", values);
  }
  return qtype;
}

static gboolean
gst_qticodec2_caps_has_feature (const GstCaps * caps, const gchar * partten)
{
  guint count = gst_caps_get_size (caps);
  gboolean ret = FALSE;

  if (count > 0) {
    for (gint i = 0; i < count; i++) {
      GstCapsFeatures *features = gst_caps_get_features (caps, i);
      if (gst_caps_features_is_any (features))
        continue;
      if (gst_caps_features_contains (features, partten))
        ret = TRUE;
    }
  }

  return ret;
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

    enc->comp_intf = c2component_intf(enc->comp);

    ret = c2component_setListener(enc->comp, encoder, handle_video_event, BLOCK_MODE_MAY_BLOCK);
    if (ret ==  FALSE) {
       GST_DEBUG_OBJECT (enc, "Failed to set event handler");
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

    /* Fill actual width/height into output caps */
    GValue g_width = { 0, }, g_height = { 0, };
    g_value_init (&g_width, G_TYPE_INT);
    g_value_set_int (&g_width, enc->width);

    g_value_init (&g_height, G_TYPE_INT);
    g_value_set_int (&g_height, enc->height);

    if ((enc->rotation == 90) || (enc->rotation == 270)) {
      gst_caps_set_value (outcaps, "width", &g_height);
      gst_caps_set_value (outcaps, "height", &g_width);
    } else {
      gst_caps_set_value (outcaps, "width", &g_width);
      gst_caps_set_value (outcaps, "height", &g_height);
    }

    GST_INFO_OBJECT (enc, "Fixed output caps: %" GST_PTR_FORMAT, outcaps);

    streamformat = gst_to_c2_streamformat (structure);
    if (!streamformat) {
      GST_ERROR_OBJECT (enc, "Unsupported format in caps: %" GST_PTR_FORMAT, outcaps);
      gst_caps_unref (outcaps);
      return GST_FLOW_ERROR;
    }

    enc->streamformat = streamformat;
    enc->output_state = gst_video_encoder_set_output_state (encoder, outcaps, state);
    enc->output_setup = TRUE;

    if ((enc->rotation == 90) || (enc->rotation == 270)) {
      enc->output_state->info.width = enc->height;
      enc->output_state->info.height = enc->width;
    }
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

  return ret;
}

/* Dispatch any pending remaining data at EOS. Class can refuse to decode new data after. */
static GstFlowReturn
gst_qticodec2venc_finish (GstVideoEncoder* encoder) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC(encoder);
  gint64 timeout;
  BufferDescriptor inBuf;

  memset (&inBuf, 0, sizeof(BufferDescriptor));

  GST_DEBUG_OBJECT (enc, "finish");

  inBuf.fd = -1;
  inBuf.data = NULL;
  inBuf.size = 0;
  inBuf.timestamp = 0;
  inBuf.index = enc->frame_index;
  inBuf.flag = FLAG_TYPE_END_OF_STREAM;
  inBuf.pool_type = BUFFER_POOL_BASIC_GRAPHIC;

  /* Setup EOS work */
  if (enc->comp) {
    /* Queue buffer to Codec2 */
    c2component_queue(enc->comp, &inBuf);
  }

  /* wait for all the pending buffers to return*/
  GST_VIDEO_ENCODER_STREAM_UNLOCK(encoder);

  g_mutex_lock (&enc->pending_lock);
  if (!enc->eos_reached) {
    GST_DEBUG_OBJECT(enc, "wait until EOS signal is triggered");

    timeout  = g_get_monotonic_time() + (EOS_WAITING_TIMEOUT * G_TIME_SPAN_SECOND);
    if (!g_cond_wait_until (&enc->pending_cond, &enc->pending_lock, timeout)) {
      GST_ERROR_OBJECT(enc, "Timed out on wait, exiting!");
    }
  }
  else {
    GST_DEBUG_OBJECT(enc, "EOS reached on output, finish the decoding");
  }

  g_mutex_unlock (&enc->pending_lock);
  GST_VIDEO_ENCODER_STREAM_LOCK(encoder);

  return GST_FLOW_OK;
}

/* Called to inform the caps describing input video data that encoder is about to receive.
  Might be called more than once, if changing input parameters require reconfiguration. */
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
  GPtrArray* config = NULL;
  ConfigParams resolution;
  ConfigParams interlace;
  ConfigParams pixelformat;
  ConfigParams mirror;
  ConfigParams rotation;
  ConfigParams rate_control;
  ConfigParams downscale;
  ConfigParams color_space_conversion;
  ConfigParams color_aspects;
  ConfigParams intra_refresh;
  ConfigParams bitrate;
  ConfigParams slice_mode;

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
    GST_ERROR_OBJECT (enc, "fail to setup output");
    goto error_output;
  }

  if (!gst_video_encoder_negotiate (encoder)) {
    GST_ERROR_OBJECT (enc,
        "Failed to negotiate with downstream");
    goto error_output;
  }

  config = g_ptr_array_new ();

  if (enc->target_bitrate > 0) {
    bitrate = make_bitrate_param (enc->target_bitrate, FALSE);
    g_ptr_array_add (config, &bitrate);
    GST_DEBUG_OBJECT (enc, "set target bitrate:%u", enc->target_bitrate);
  }

  resolution = make_resolution_param(width, height, TRUE);
  g_ptr_array_add (config, &resolution);

  pixelformat = make_pixelFormat_param(gst_to_c2_pixelformat(input_format), TRUE);
  g_ptr_array_add (config, &pixelformat);

  rate_control = make_rateControl_param (enc->rcMode);
  g_ptr_array_add (config, &rate_control);

  if (enc->mirror != MIRROR_NONE) {
    mirror = make_mirror_param (enc->mirror, TRUE);
    g_ptr_array_add (config, &mirror);
  }

  if (enc->rotation > 0) {
    rotation = make_rotation_param (enc->rotation, TRUE);
    g_ptr_array_add (config, &rotation);
  }

  if (enc->downscale_width > 0 && enc->downscale_height > 0) {
    downscale = make_downscale_param (enc->downscale_width, enc->downscale_height);
    g_ptr_array_add (config, &downscale);
  }

  if (enc->slice_mode != SLICE_MODE_DISABLE) {
    slice_mode = make_slicemode_param(enc->slice_size, enc->slice_mode);
    g_ptr_array_add (config, &slice_mode);
  }

  if (enc->color_space_conversion) {
    GST_DEBUG_OBJECT (enc, "enable color space conversion");
    color_space_conversion = make_colorSpaceConv_param (enc->color_space_conversion);
    g_ptr_array_add (config, &color_space_conversion);
    GST_DEBUG_OBJECT (enc, "set color aspect info");
    color_aspects = make_colorAspects_param (enc->primaries, enc->transfer_char, enc->matrix, enc->full_range);
    g_ptr_array_add (config, &color_aspects);
  }

  if (enc->intra_refresh_mode && enc->intra_refresh_mbs) {
    GST_DEBUG_OBJECT (enc, "set intra refresh mode: %d, mbs:%d", enc->intra_refresh_mode, enc->intra_refresh_mbs);
    intra_refresh = make_intraRefresh_param (enc->intra_refresh_mode, enc->intra_refresh_mbs);
    g_ptr_array_add (config, &intra_refresh);
  }

  /* Create component */
  if (!gst_qticodec2venc_create_component (encoder)){
    GST_ERROR_OBJECT(enc, "Failed to create component");
  }

  GST_DEBUG_OBJECT (enc, "set graphic pool with: %d, height: %d, format: %x, rc mode: %d",
      enc->width, enc->height, enc->input_format, enc->rcMode);

  if (!c2componentInterface_config(
        enc->comp_intf,
        config,
        BLOCK_MODE_MAY_BLOCK)) {
      GST_WARNING_OBJECT (enc, "Failed to set encoder config");
  }

  g_ptr_array_free (config, FALSE);

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

  GST_DEBUG ("Frame number : %d, pts: %" GST_TIME_FORMAT,
    frame->system_frame_number, GST_TIME_ARGS (frame->pts));

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
  GstPad* pad = GST_VIDEO_ENCODER_SRC_PAD (encoder);
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
  GstPad* pad = GST_VIDEO_ENCODER_SINK_PAD (encoder);
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (enc, "sink_query of type '%s'",gst_query_type_get_name (GST_QUERY_TYPE (query)));

  switch (GST_QUERY_TYPE (query)) {
    default:
      ret = GST_VIDEO_ENCODER_CLASS (parent_class)->sink_query (encoder, query);
      break;
  }

  return ret;
}

static gboolean
gst_qticodec2venc_propose_allocation(GstVideoEncoder * encoder, GstQuery * query) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
  GstCaps *caps;
  GstVideoInfo info;
  GstAllocator *allocator = NULL;
  guint num_max_buffers = MAX_INPUT_BUFFERS;

  gst_query_parse_allocation (query, &caps, NULL);

  if (!caps) {
    GST_INFO_OBJECT (encoder, "failed to get caps");
    goto cleanup;
  }

  GST_INFO_OBJECT (enc, "allocation caps: %" GST_PTR_FORMAT, caps);

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_INFO_OBJECT (encoder, "failed to get video info");
    goto cleanup;
  }

  /* Propose GBM backed memory if upstream has dmabuf feature */
  if (gst_qticodec2_caps_has_feature(caps, GST_CAPS_FEATURE_MEMORY_DMABUF)) {
    enc->pool = gst_qticodec2_buffer_pool_new (enc->comp, BUFFER_POOL_BASIC_GRAPHIC,
        num_max_buffers, caps);

    if(!enc->pool)
      goto cleanup;

    if (enc->pool) {
      GstStructure *config;
      GstAllocationParams params = { 0, 0, 0, 0, };

      config = gst_buffer_pool_get_config (GST_BUFFER_POOL_CAST (enc->pool));

      if (!gst_buffer_pool_config_get_allocator (config, &allocator, NULL)) {
        gst_structure_free (config);
        GST_ERROR_OBJECT (enc, "failed to get allocator from pool");
        goto cleanup;
      } else {
        gst_query_add_allocation_param (query, allocator, &params);
      }

      gst_structure_free (config);

      /* add pool into allocation query */
      gst_query_add_allocation_pool (query, enc->pool, GST_VIDEO_INFO_SIZE (&info),
          0, num_max_buffers);
      gst_object_unref (enc->pool);
    }
  } else {
    GST_INFO_OBJECT (enc, "peer component does not support dmabuf feature: %" GST_PTR_FORMAT, caps);
  }

  return GST_VIDEO_ENCODER_CLASS (parent_class)->propose_allocation (encoder, query);

cleanup:
  if (enc->pool)
    gst_object_unref(enc->pool);

  return FALSE;
}

/* Push decoded frame to downstream element */
static GstFlowReturn
push_frame_downstream(GstVideoEncoder* encoder, BufferDescriptor* encode_buf) {
  Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoCodecFrame* frame = NULL;
  GstMapInfo map = GST_MAP_INFO_INIT;
  GstBuffer* outbuf = NULL;
  GstVideoCodecState *state = NULL;
  GstVideoInfo *vinfo = NULL;
  GstStructure* structure = NULL;

  GST_DEBUG_OBJECT (enc, "push_frame_downstream");

  state = gst_video_encoder_get_output_state (encoder);
  if (state) {
    vinfo = &state->info;
  } else {
    GST_ERROR_OBJECT (enc, "video codec state is NULL, unexpected!");
    goto out;
  }

  frame = gst_video_encoder_get_frame (encoder, encode_buf->index);
  if (frame == NULL) {
    GST_ERROR_OBJECT (enc, "Error in gst_video_encoder_get_frame, frame number: %lu",
        encode_buf->index);
    goto out;
  }

  /* If using our own buffer pool, unref the corresponding input buffer
   * so that it can be returned into the pool
   * */
  if (enc->pool) {
    GST_DEBUG_OBJECT (enc, "unref input buffer: %p", frame->input_buffer);
    gst_buffer_unref (frame->input_buffer);
  }

  if (encode_buf->flag & FLAG_TYPE_CODEC_CONFIG) {
    GST_DEBUG_OBJECT (enc, "fill codec config size:%d first frame size:%d", encode_buf->config_size, encode_buf->size);
    outbuf = gst_buffer_new_and_alloc (encode_buf->size + encode_buf->config_size);
    gst_buffer_fill(outbuf, 0, encode_buf->config_data, encode_buf->config_size);
    gst_buffer_fill(outbuf, encode_buf->config_size, encode_buf->data, encode_buf->size);
  } else {
    outbuf = gst_buffer_new_and_alloc (encode_buf->size);
    gst_buffer_fill(outbuf, 0, encode_buf->data, encode_buf->size);
  }

  if (outbuf) {
    gst_buffer_set_flags (outbuf, GST_BUFFER_FLAG_SYNC_AFTER);
    GST_BUFFER_TIMESTAMP (outbuf) = gst_util_uint64_scale(encode_buf->timestamp, GST_SECOND,
        C2_TICKS_PER_SECOND);

    if (vinfo->fps_n > 0) {
      GST_BUFFER_DURATION (outbuf) = gst_util_uint64_scale(GST_SECOND,
          vinfo->fps_d, vinfo->fps_n);
    }

    GST_DEBUG_OBJECT (enc, "out buffer: %p, PTS: %lu, duration: %lu, fps_d: %d, fps_n: %d",
        outbuf, GST_BUFFER_PTS (outbuf), GST_BUFFER_DURATION (outbuf), vinfo->fps_d, vinfo->fps_n);

    /* Creates a new, empty GstStructure with the given name */
    structure = gst_structure_new_empty("BUFFER");
    gst_structure_set (structure,
        "encoder", G_TYPE_POINTER, encoder,
        "index", G_TYPE_UINT64, encode_buf->index,
        NULL);
    /* Set a notification function to signal when the buffer is no longer used. */
    gst_mini_object_set_qdata (GST_MINI_OBJECT (outbuf), qticodec2venc_qdata_quark (),
        structure, (GDestroyNotify)gst_qticodec2venc_buffer_release);

    frame->output_buffer = outbuf;
    gst_video_codec_frame_unref (frame);
    ret = gst_video_encoder_finish_frame (encoder, frame);
    if(ret != GST_FLOW_OK){
      GST_ERROR_OBJECT (enc, "Failed to finish frame, outbuf: %p", outbuf);
      goto out;
    }
  } else {
    GST_ERROR_OBJECT (enc, "Failed to create outbuf");
    goto out;
  }

  return ret;

out:
  return GST_FLOW_ERROR;
}

static void
gst_qticodec2venc_buffer_release (GstStructure* structure)
{
  GstVideoEncoder *encoder = NULL;
  Gstqticodec2venc* dec = NULL;
  guint64 index = 0;

  gst_structure_get(structure, "encoder", G_TYPE_POINTER, &encoder, NULL);
  gst_structure_get_uint64 (structure, "index", &index);

  if (encoder) {
    Gstqticodec2venc* enc = GST_QTICODEC2VENC (encoder);

    GST_LOG_OBJECT (enc, "gst_qticodec2venc_buffer_release");

    if (!c2component_freeOutBuffer(enc->comp, index)) {
      GST_ERROR_OBJECT (dec, "Failed to release the buffer (%lu)", index);
    }
  } else{
    GST_ERROR_OBJECT (dec, "Null handle");
  }

  gst_structure_free (structure);
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
      BufferDescriptor* outBuffer = (BufferDescriptor*)data;

      GST_DEBUG_OBJECT (enc, "Event output done, va: %p, offset: %d, index: %lu, fd: %u, \
          filled len: %u, buffer size: %u, timestamp: %lu, flag: %x", outBuffer->data,
          outBuffer->offset, outBuffer->index, outBuffer->fd, outBuffer->size,
          outBuffer->capacity, outBuffer->timestamp, outBuffer->flag);

      if (outBuffer->fd > 0 || outBuffer->size > 0) {
        ret = push_frame_downstream (encoder, outBuffer);
        if (ret != GST_FLOW_OK) {
          GST_ERROR_OBJECT (enc, "Failed to push frame downstream");
        }
      } else if (outBuffer->flag & FLAG_TYPE_END_OF_STREAM) {
        GST_INFO_OBJECT (enc, "Encoder reached EOS");
        g_mutex_lock (&enc->pending_lock);
        enc->eos_reached = TRUE;
        g_cond_signal(&enc->pending_cond);
        g_mutex_unlock (&enc->pending_lock);
      } else {
          GST_ERROR_OBJECT (enc, "Invalid output buffer");
      }
      break;
    }
    case EVENT_TRIPPED: {
      GST_ERROR_OBJECT (enc, "EVENT_TRIPPED(%d)", *(gint32*)data);
      break;
    }
    case EVENT_ERROR: {
      GST_ERROR_OBJECT (enc, "EVENT_ERROR(%d)", *(gint32*)data);
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
  BufferDescriptor inBuf;
  GstBuffer* buf = NULL;
  GstMemory *mem;
  GstMapInfo mapinfo = { 0, };
  gboolean mem_mapped = FALSE;
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (enc, "encode");

  memset (&inBuf, 0, sizeof(BufferDescriptor));
  inBuf.flag = 0;

  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
  if (frame) {
    buf = frame->input_buffer;
    mem = gst_buffer_get_memory (buf, 0);

    if (gst_is_dmabuf_memory (mem)) {
      inBuf.fd = gst_dmabuf_memory_get_fd (mem);
      inBuf.size = gst_memory_get_sizes (mem, NULL, NULL);;
      inBuf.data = NULL;
      inBuf.c2_buffer = gst_mini_object_get_qdata (GST_MINI_OBJECT (buf), qticodec2_c2buf_qdata_quark ());
      GST_DEBUG_OBJECT (enc, "input c2 buffer:%p fd:%d", inBuf.c2_buffer, inBuf.fd);
    } else {
      gst_buffer_map (buf, &mapinfo, GST_MAP_READ);
      mem_mapped = TRUE;
      inBuf.fd = -1;
      inBuf.data = mapinfo.data;
      inBuf.size = mapinfo.size;
    }

    inBuf.timestamp = NANO_TO_MILLI(frame->pts);
    inBuf.index = frame->system_frame_number;
    inBuf.pool_type = BUFFER_POOL_BASIC_GRAPHIC;
    inBuf.width = enc->width;
    inBuf.height = enc->height;
    inBuf.format = enc->input_format;

    gst_memory_unref (mem);

    GST_DEBUG_OBJECT (enc, "input buffer: fd: %d, va:%p, size: %d, timestamp: %lu, index: %ld",
      inBuf.fd, inBuf.data, inBuf.size, inBuf.timestamp, inBuf.index);
  }

  /* Keep track of queued frame */
  enc->queued_frame[(enc->frame_index) % MAX_QUEUED_FRAME] =  frame->system_frame_number;

  /* Queue buffer to Codec2 */
  ret = c2component_queue (enc->comp, &inBuf);
  /* unmap the gstbuffer if it's mapped*/
  if (mem_mapped) {
    gst_buffer_unmap (buf, &mapinfo);
  }

  if (!ret) {
    goto error_setup_input;
  }

  g_mutex_lock (&(enc->pending_lock));
  enc->frame_index += 1;
  enc->num_input_queued++;
  g_mutex_unlock (&(enc->pending_lock));

  /* ref the buffer so that the it will not be returned to the pool */
  if (enc->pool) {
    GST_DEBUG_OBJECT (enc, "ref input buffer: %p", frame->input_buffer);
    gst_buffer_ref (frame->input_buffer);
  }

  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return GST_FLOW_OK;

error_setup_input:
  GST_ERROR_OBJECT(enc, "failed to setup input");
  return GST_FLOW_ERROR;
}

static void
gst_qticodec2venc_set_property (GObject* object, guint prop_id,
    const GValue* value, GParamSpec* pspec) {

  Gstqticodec2venc* enc = GST_QTICODEC2VENC (object);

  GST_DEBUG_OBJECT (enc, "qticodec2venc_set_property");

  switch (prop_id) {
    case PROP_SILENT:
      enc->silent = g_value_get_boolean (value);
      break;
    case PROP_MIRROR:
      enc->mirror = g_value_get_enum (value);
      break;
    case PROP_ROTATION:
      enc->rotation = g_value_get_uint (value);
      break;
    case PROP_RATE_CONTROL:
      enc->rcMode = g_value_get_enum (value);
      break;
    case PROP_DOWNSCALE_WIDTH:
      enc->downscale_width = g_value_get_uint (value);
      break;
    case PROP_DOWNSCALE_HEIGHT:
      enc->downscale_height = g_value_get_uint (value);
      break;
    case PROP_COLOR_SPACE_PRIMARIES:
      enc->primaries = g_value_get_enum (value);
      break;
    case PROP_COLOR_SPACE_MATRIX_COEFFS:
      enc->matrix = g_value_get_enum (value);
      break;
    case PROP_COLOR_SPACE_TRANSFER_CHAR:
      enc->transfer_char = g_value_get_enum (value);
      break;
    case PROP_COLOR_SPACE_FULL_RANGE:
      enc->full_range = g_value_get_enum (value);
      break;
    case PROP_COLOR_SPACE_CONVERSION:
      enc->color_space_conversion = g_value_get_boolean (value);
      break;
    case PROP_INTRA_REFRESH_MODE:
      enc->intra_refresh_mode = g_value_get_enum (value);
      break;
    case PROP_INTRA_REFRESH_MBS:
      enc->intra_refresh_mbs = g_value_get_uint (value);
      break;
    case PROP_TARGET_BITRATE:
      enc->target_bitrate = g_value_get_uint (value);
      break;
    case PROP_SLICE_SIZE:
      enc->slice_size = g_value_get_uint (value);
      break;
    case PROP_SLICE_MODE:
      enc->slice_mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_qticodec2venc_get_property (GObject* object, guint prop_id,
    GValue* value, GParamSpec* pspec) {

  Gstqticodec2venc* enc = GST_QTICODEC2VENC (object);

  GST_DEBUG_OBJECT (enc, "qticodec2venc_get_property");

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, enc->silent);
      break;
    case PROP_MIRROR:
      g_value_set_enum (value, enc->mirror);
      break;
    case PROP_ROTATION:
      g_value_set_uint (value, enc->rotation);
      break;
    case PROP_RATE_CONTROL:
      g_value_set_enum (value, enc->rcMode);
      break;
    case PROP_DOWNSCALE_WIDTH:
      g_value_set_uint (value, enc->downscale_width);
      break;
    case PROP_DOWNSCALE_HEIGHT:
      g_value_set_uint (value, enc->downscale_height);
      break;
    case PROP_COLOR_SPACE_PRIMARIES:
      g_value_set_enum (value, enc->primaries);
      break;
    case PROP_COLOR_SPACE_MATRIX_COEFFS:
      g_value_set_enum (value, enc->matrix);
      break;
    case PROP_COLOR_SPACE_TRANSFER_CHAR:
      g_value_set_enum (value, enc->transfer_char);
      break;
    case PROP_COLOR_SPACE_FULL_RANGE:
      g_value_set_enum (value, enc->full_range);
      break;
    case PROP_COLOR_SPACE_CONVERSION:
      g_value_set_boolean (value, enc->color_space_conversion);
      break;
    case PROP_INTRA_REFRESH_MODE:
      g_value_set_enum (value, enc->intra_refresh_mode);
      break;
    case PROP_INTRA_REFRESH_MBS:
      g_value_set_uint (value, enc->intra_refresh_mbs);
      break;
    case PROP_TARGET_BITRATE:
      g_value_set_uint (value, enc->target_bitrate);
      break;
    case PROP_SLICE_SIZE:
      g_value_set_uint (value, enc->slice_size);
      break;
    case PROP_SLICE_MODE:
      g_value_set_enum (value, enc->slice_mode);
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

  g_object_class_install_property (gobject_class, PROP_RATE_CONTROL,
      g_param_spec_enum ("rate-control", "Rate Control",
          "Bitrate control method",
          GST_TYPE_CODEC2_ENC_RATE_CONTROL,
          RC_OFF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_MIRROR,
      g_param_spec_enum ("mirror", "Mirror Type",
          "Specify the mirror type",
          GST_TYPE_CODEC2_ENC_MIRROR_TYPE,
          MIRROR_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_ROTATION,
      g_param_spec_uint ("rotation", "Rotation",
          "Specify the angle of clockwise rotation. [0|90|180|270]",
          0, 270, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (G_OBJECT_CLASS(klass), PROP_DOWNSCALE_WIDTH,
      g_param_spec_uint ("downscale-width", "Downscale width", "Specify the downscale width",
          0, UINT_MAX, 0, G_PARAM_READWRITE));

  g_object_class_install_property (G_OBJECT_CLASS(klass), PROP_DOWNSCALE_HEIGHT,
      g_param_spec_uint ("downscale-height", "Downscale height", "Specify the downscale height",
          0, UINT_MAX, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_COLOR_SPACE_PRIMARIES,
      g_param_spec_enum ("color-primaries", "Input colour primaries",
          "Chromaticity coordinates of the source primaries",
          GST_TYPE_CODEC2_ENC_COLOR_PRIMARIES,
          COLOR_PRIMARIES_UNSPECIFIED,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_COLOR_SPACE_MATRIX_COEFFS,
      g_param_spec_enum ("matrix-coeffs", "Input matrix coefficients",
          "Matrix coefficients used in deriving luma and chroma signals from RGB primaries",
          GST_TYPE_CODEC2_ENC_MATRIX_COEFFS,
          COLOR_MATRIX_UNSPECIFIED,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_COLOR_SPACE_TRANSFER_CHAR,
      g_param_spec_enum ("transfer-char", "Input transfer characteristics",
          "The opto-electronic transfer characteristics to use.",
          GST_TYPE_CODEC2_ENC_TRANSFER_CHAR,
          COLOR_TRANSFER_UNSPECIFIED,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_COLOR_SPACE_FULL_RANGE,
      g_param_spec_enum ("full-range", "Full range flag",
          "Black level and range of the luma and chroma signals.",
          GST_TYPE_CODEC2_ENC_FULL_RANGE,
          COLOR_RANGE_UNSPECIFIED,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (G_OBJECT_CLASS(klass), PROP_COLOR_SPACE_CONVERSION,
      g_param_spec_boolean ("color-space-conversion", "Color space conversion",
          "If enabled, should be in color space conversion mode",
          GST_QTI_CODEC2_ENC_COLOR_SPACE_CONVERSION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_INTRA_REFRESH_MODE,
      g_param_spec_enum ("intra-refresh-mode", "Intra refresh mode",
          "Intra refresh mode, only support random mode. Allow IR only for CBR(_CFR/VFR) RC modes",
          GST_TYPE_CODEC2_ENC_INTRA_REFRESH_MODE,
          IR_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_INTRA_REFRESH_MBS,
      g_param_spec_uint ("intra-refresh-mbs", "Intra refresh mbs/period",
          "For random modes, it means period of intra refresh. Only support random mode.",
          0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (G_OBJECT_CLASS(klass), PROP_TARGET_BITRATE,
      g_param_spec_uint ("target-bitrate", "Target bitrate",
          "Target bitrate in bits per second (0 means not explicitly set bitrate)",
          0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_SLICE_MODE,
      g_param_spec_enum ("slice-mode", "slice mode",
          "Slice mode, support MB and BYTES mode",
          GST_TYPE_CODEC2_ENC_SLICE_MODE,
          SLICE_MODE_DISABLE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (G_OBJECT_CLASS(klass), PROP_SLICE_SIZE,
      g_param_spec_uint ("slice-size", "Slice size",
          "Slice size, just set when slice mode setting to MB or Bytes",
          0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));


  video_encoder_class->start = GST_DEBUG_FUNCPTR (gst_qticodec2venc_start);
  video_encoder_class->stop = GST_DEBUG_FUNCPTR (gst_qticodec2venc_stop);
  video_encoder_class->set_format = GST_DEBUG_FUNCPTR (gst_qticodec2venc_set_format);
  video_encoder_class->handle_frame =  GST_DEBUG_FUNCPTR (gst_qticodec2venc_handle_frame);
  video_encoder_class->finish = GST_DEBUG_FUNCPTR (gst_qticodec2venc_finish);
  video_encoder_class->open = GST_DEBUG_FUNCPTR (gst_qticodec2venc_open);
  video_encoder_class->close = GST_DEBUG_FUNCPTR (gst_qticodec2venc_close);
  video_encoder_class->src_query = GST_DEBUG_FUNCPTR (gst_qticodec2venc_src_query);
  video_encoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_qticodec2venc_sink_query);
  video_encoder_class->propose_allocation = GST_DEBUG_FUNCPTR (gst_qticodec2venc_propose_allocation);

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
  enc->eos_reached = FALSE;
  enc->input_state = NULL;
  enc->output_state = NULL;
  enc->pool = NULL;
  enc->width = 0;
  enc->height = 0;
  enc->frame_index = 0;
  enc->num_input_queued = 0;
  enc->num_output_done = 0;
  enc->rcMode = RC_OFF;
  enc->mirror = MIRROR_NONE;
  enc->rotation = 0;
  enc->downscale_width = 0;
  enc->downscale_height = 0;
  enc->target_bitrate = 0;

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
