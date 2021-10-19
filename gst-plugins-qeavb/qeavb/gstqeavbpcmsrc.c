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

#include <stdio.h>
#include "gstqeavbpcmsrc.h"

GST_DEBUG_CATEGORY_STATIC (qeavbpcmsrc_debug);
#define GST_CAT_DEFAULT (qeavbpcmsrc_debug)

#define DEFAULT_PCM_CONFIG_FILE "/etc/xdg/listenerPCM.ini"
#define DEFAULT_PCMSRC_IS_LIVE TRUE
enum
{
  PROP_0,
  PROP_CONFIG_FILE,
  PROP_IS_LIVE,
};

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_FORMATS_ALL ", "
        "layout = (string) { interleaved, non-interleaved }, "
        "rate = " GST_AUDIO_RATE_RANGE ", "
        "channels = " GST_AUDIO_CHANNELS_RANGE)
    );

#define gst_qeavb_pcm_src_parent_class parent_class
G_DEFINE_TYPE (GstQeavbPcmSrc, gst_qeavb_pcm_src, GST_TYPE_PUSH_SRC);

static void gst_qeavb_pcm_src_finalize (GObject * gobject);

static void gst_qeavb_pcm_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_qeavb_pcm_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_qeavb_pcm_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_qeavb_pcm_src_start (GstBaseSrc * basesrc);
static gboolean gst_qeavb_pcm_src_stop (GstBaseSrc * basesrc);
static GstFlowReturn gst_qeavb_pcm_src_fill (GstPushSrc * pushsrc, GstBuffer *
    buffer);
static GstStateChangeReturn
    gst_qeavb_pcm_src_change_state (GstElement * element,
    GstStateChange transition);

static void
gst_qeavb_pcm_src_class_init (GstQeavbPcmSrcClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS (klass);

  object_class->finalize = gst_qeavb_pcm_src_finalize;
  object_class->get_property = gst_qeavb_pcm_src_get_property;
  object_class->set_property = gst_qeavb_pcm_src_set_property;
  element_class->change_state = gst_qeavb_pcm_src_change_state;

  gst_element_class_add_static_pad_template (element_class, &src_template);
  g_object_class_install_property (object_class, PROP_CONFIG_FILE,
      g_param_spec_string ("config-file", "Config File Name",
          "Config file name to config eavb",
          DEFAULT_PCM_CONFIG_FILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (object_class, PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Whether to act as a live source", DEFAULT_PCMSRC_IS_LIVE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  gst_element_class_set_static_metadata (element_class,
      "Audio PCM Transport Source",
      "Src/Network", "Receive audio pcm from the network",
      "Lily Li <lali@codeaurora.org>");

  basesrc_class->start = GST_DEBUG_FUNCPTR (gst_qeavb_pcm_src_start);
  basesrc_class->stop = GST_DEBUG_FUNCPTR (gst_qeavb_pcm_src_stop);
  pushsrc_class->fill = GST_DEBUG_FUNCPTR (gst_qeavb_pcm_src_fill);
  basesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_qeavb_pcm_src_fixate);
}

static void
gst_qeavb_pcm_src_init (GstQeavbPcmSrc * qeavbpcmsrc)
{
  gst_base_src_set_live (GST_BASE_SRC (qeavbpcmsrc), DEFAULT_PCMSRC_IS_LIVE);
  gst_base_src_set_format (GST_BASE_SRC (qeavbpcmsrc), DEFAULT_PCMSRC_IS_LIVE ? GST_FORMAT_TIME : GST_FORMAT_BYTES);
  gst_base_src_set_blocksize (GST_BASE_SRC (qeavbpcmsrc), QEAVB_PCM_DEFAULT_BLOCKSIZE);

  qeavbpcmsrc->config_file = g_strdup (DEFAULT_PCM_CONFIG_FILE);
  qeavbpcmsrc->eavb_addr = NULL;
  qeavbpcmsrc->eavb_fd = -1;
  qeavbpcmsrc->started = FALSE;
  qeavbpcmsrc->is_first_pcmpacket = TRUE;
  memset(&(qeavbpcmsrc->hdr), 0, sizeof(eavb_ioctl_hdr_t));
  memset(&(qeavbpcmsrc->stream_info), 0, sizeof(eavb_ioctl_stream_info_t));
  g_mutex_init (&qeavbpcmsrc->lock);
  kpi_place_marker("M - qeavbpcmsrc init");
}

static void
gst_qeavb_pcm_src_finalize (GObject * object)
{
  GstQeavbPcmSrc *qeavbpcmsrc = GST_QEAVB_PCM_SRC (object);
  g_free(qeavbpcmsrc->config_file);
  g_mutex_clear (&qeavbpcmsrc->lock);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_qeavb_pcm_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstQeavbPcmSrc *qeavbpcmsrc = GST_QEAVB_PCM_SRC (object);
  gboolean is_live = FALSE;

  GST_DEBUG_OBJECT (qeavbpcmsrc, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_CONFIG_FILE:
      g_free (qeavbpcmsrc->config_file);
      qeavbpcmsrc->config_file = g_value_dup_string (value);
      break;
    case PROP_IS_LIVE:
      is_live = g_value_get_boolean (value);
      gst_base_src_set_live (GST_BASE_SRC (qeavbpcmsrc), is_live);
      gst_base_src_set_format (GST_BASE_SRC (qeavbpcmsrc), is_live ? GST_FORMAT_TIME : GST_FORMAT_BYTES);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_qeavb_pcm_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstQeavbPcmSrc *qeavbpcmsrc = GST_QEAVB_PCM_SRC (object);

  GST_DEBUG_OBJECT (qeavbpcmsrc, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_CONFIG_FILE:
      g_value_set_string (value, qeavbpcmsrc->config_file);
      break;
    case PROP_IS_LIVE:
      g_value_set_boolean (value, gst_base_src_is_live (GST_BASE_SRC (qeavbpcmsrc)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_qeavb_pcm_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstQeavbPcmSrc *src = GST_QEAVB_PCM_SRC (bsrc);
  GstStructure *structure;
  gint channels, rate;
  unsigned char endianness = src->stream_info.endianness;
  int bit_depth = src->stream_info.pcm_bit_depth;

  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (src, "fixating samplerate to %d", GST_AUDIO_DEF_RATE);

  if(src->stream_info.sample_rate)
    rate = src->stream_info.sample_rate;
  else
    rate = GST_AUDIO_DEF_RATE;

  gst_structure_fixate_field_nearest_int (structure, "rate", rate);
  GST_DEBUG_OBJECT (bsrc, "rate %d", rate);
  GST_DEBUG_OBJECT (bsrc, "endianness %d, bit_depth %d", endianness, bit_depth);
  if(bit_depth == 16) {
    if (endianness)
      gst_structure_fixate_field_string (structure, "format", "S16LE");
    else
      gst_structure_fixate_field_string (structure, "format", "S16BE");
  } else if (bit_depth == 32) {
    if (endianness)
      gst_structure_fixate_field_string (structure, "format", "S32LE");
    else
      gst_structure_fixate_field_string (structure, "format", "S32BE");
  } else if (bit_depth == 24) {
    if (endianness)
      gst_structure_fixate_field_string (structure, "format", "S24LE");
    else
      gst_structure_fixate_field_string (structure, "format", "S24BE");
  } else if (bit_depth == 8) {
    gst_structure_fixate_field_string (structure, "format", "S8");
  } else {
    gst_structure_fixate_field_string (structure, "format", GST_AUDIO_DEF_FORMAT);
  }

  gst_structure_fixate_field_string (structure, "layout", "interleaved");

  /* fixate to mono unless downstream requires stereo, for backwards compat */
  gst_structure_fixate_field_nearest_int (structure, "channels", src->stream_info.num_pcm_channels);
  GST_DEBUG_OBJECT (bsrc, "pcm channels %d", src->stream_info.num_pcm_channels);
  if (gst_structure_get_int (structure, "channels", &channels) && channels > 2) {
    if (!gst_structure_has_field_typed (structure, "channel-mask",
            GST_TYPE_BITMASK))
      gst_structure_set (structure, "channel-mask", GST_TYPE_BITMASK, 0,
          NULL);
  }

  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);

  return caps;
}

static GstStateChangeReturn
gst_qeavb_pcm_src_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstQeavbPcmSrc *src = GST_QEAVB_PCM_SRC (element);
  GST_INFO_OBJECT(src, "Doing self transition: 0x%x(%s)", transition, gst_state_change_get_name(transition));
  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_qeavb_pcm_src_stop(GST_BASE_SRC (src));
    break;

    default:
    break;
  }

  GST_INFO_OBJECT(src, "Doing parent transition: 0x%x", transition);
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  GST_INFO_OBJECT(src, "Done transition: 0x%x, ret %d(%s)", transition, ret, gst_element_state_change_return_get_name(ret));
  return ret;
}

static gboolean
gst_qeavb_pcm_src_start (GstBaseSrc * basesrc)
{
  int err = 0;
  GstQeavbPcmSrc *qeavbpcmsrc = GST_QEAVB_PCM_SRC (basesrc);

  GST_INFO_OBJECT(qeavbpcmsrc,"qeavb pcm src start");
  kpi_place_marker("M - qeavbpcmsrc start");
  qeavbpcmsrc->eavb_fd = open("/dev/virt-eavb", O_RDWR);
  if (qeavbpcmsrc->eavb_fd < 0) {
    GST_ERROR_OBJECT (qeavbpcmsrc,"open eavb fd error, exit!");
    return FALSE;
  }

  err = qeavb_create_stream_remote(qeavbpcmsrc->eavb_fd, qeavbpcmsrc->config_file, &(qeavbpcmsrc->hdr));
  if (0 != err) {
    GST_ERROR_OBJECT (qeavbpcmsrc,"create stream remote error %d, exit!", err);
    goto error_close;
  }

  err = qeavb_connect_stream(qeavbpcmsrc->eavb_fd, &(qeavbpcmsrc->hdr));
  if (0 != err) {
    GST_ERROR_OBJECT (qeavbpcmsrc,"connect stream error %d, exit!", err);
    goto error_destroy;
  }
  GST_DEBUG_OBJECT (qeavbpcmsrc,"get stream info");

  err = qeavb_get_stream_info(qeavbpcmsrc->eavb_fd, &(qeavbpcmsrc->hdr), &(qeavbpcmsrc->stream_info));
  if (0 != err) {
    GST_ERROR_OBJECT (qeavbpcmsrc,"get stream info error %d, exit!", err);
    goto error_disconnect;
  }
  GST_DEBUG_OBJECT (qeavbpcmsrc, "QEAVB PCM source stream info pcm_bit_depth %d, num_pcm_channels %d, sample_rate %d, endianness %d, max_buffer_size %d, pkts_per_wake %d", qeavbpcmsrc->stream_info.pcm_bit_depth,
    qeavbpcmsrc->stream_info.num_pcm_channels, qeavbpcmsrc->stream_info.sample_rate, qeavbpcmsrc->stream_info.endianness, qeavbpcmsrc->stream_info.max_buffer_size, qeavbpcmsrc->stream_info.pkts_per_wake);

  if (0 != qeavbpcmsrc->stream_info.max_buffer_size && 0 != qeavbpcmsrc->stream_info.pkts_per_wake)
    gst_base_src_set_blocksize (basesrc, qeavbpcmsrc->stream_info.max_buffer_size * qeavbpcmsrc->stream_info.pkts_per_wake);
  // mmap
  qeavbpcmsrc->eavb_addr = mmap(NULL, qeavbpcmsrc->stream_info.max_buffer_size * qeavbpcmsrc->stream_info.pkts_per_wake, PROT_READ | PROT_WRITE, MAP_SHARED, qeavbpcmsrc->eavb_fd, 0);
  qeavbpcmsrc->started = TRUE;
  kpi_place_marker("M - qeavbpcmsrc started successful");
  GST_DEBUG_OBJECT (qeavbpcmsrc, "QEAVB PCM source started");
  return TRUE;

error_disconnect:
  err = qeavb_disconnect_stream(qeavbpcmsrc->eavb_fd, &(qeavbpcmsrc->hdr));
    if (0 != err) {
      GST_ERROR_OBJECT (qeavbpcmsrc,"disconnect stream error %d!", err);
    }
error_destroy:
    GST_DEBUG_OBJECT (qeavbpcmsrc,"destroying stream");
    err = qeavb_destroy_stream(qeavbpcmsrc->eavb_fd, &(qeavbpcmsrc->hdr));
    if (0 != err) {
      GST_ERROR_OBJECT (qeavbpcmsrc,"destroy stream error %d!", err);
    }
error_close:
  if (qeavbpcmsrc->eavb_fd >= 0) {
    close(qeavbpcmsrc->eavb_fd);
    qeavbpcmsrc->eavb_fd = -1;
  }
  return FALSE;
}

static gboolean
gst_qeavb_pcm_src_stop (GstBaseSrc * basesrc)
{
  int err = 0;
  GstQeavbPcmSrc *qeavbpcmsrc = GST_QEAVB_PCM_SRC (basesrc);

  g_mutex_lock(&qeavbpcmsrc->lock);
  if (qeavbpcmsrc->started) {
    munmap(qeavbpcmsrc->eavb_addr, qeavbpcmsrc->stream_info.max_buffer_size * qeavbpcmsrc->stream_info.pkts_per_wake);

    GST_DEBUG_OBJECT (qeavbpcmsrc,"desconnect stream");
    err = qeavb_disconnect_stream(qeavbpcmsrc->eavb_fd, &(qeavbpcmsrc->hdr));
    if (0 != err) {
      GST_ERROR_OBJECT (qeavbpcmsrc,"disconnect stream error %d, exit!", err);
    }

    GST_DEBUG_OBJECT (qeavbpcmsrc,"destroying stream");
    err = qeavb_destroy_stream(qeavbpcmsrc->eavb_fd, &(qeavbpcmsrc->hdr));
    if (0 != err) {
      GST_ERROR_OBJECT (qeavbpcmsrc,"destroy stream error %d, exit!", err);
    }

    close(qeavbpcmsrc->eavb_fd);
    qeavbpcmsrc->eavb_fd = -1;
    GST_DEBUG_OBJECT (qeavbpcmsrc, "QEAVB PCM source stopped");
  }
  qeavbpcmsrc->started = FALSE;
  g_mutex_unlock(&qeavbpcmsrc->lock);
  return TRUE;
}

static GstFlowReturn
gst_qeavb_pcm_src_fill (GstPushSrc * pushsrc, GstBuffer * buffer)
{
  GstMapInfo map;
  gint32 retry_time = 0;
  gint32 sleep_us = DEFALUT_SLEEP_US;
  eavb_ioctl_buf_data_t qavb_buffer;
  gint32 recv_len = 0;

  GstQeavbPcmSrc *qeavbpcmsrc = GST_QEAVB_PCM_SRC (pushsrc);
  GstFlowReturn error = GST_FLOW_OK;
  int err = 0;
  guint32 payload_size = 0;
  char tips[64];

  if(qeavbpcmsrc->stream_info.wakeup_period_us != 0)
    sleep_us = qeavbpcmsrc->stream_info.wakeup_period_us;

  payload_size = qeavbpcmsrc->stream_info.max_buffer_size * qeavbpcmsrc->stream_info.pkts_per_wake;

  GST_DEBUG_OBJECT (qeavbpcmsrc, "pkts_per_wake %d, payload_size %u, wakeup_period_us %d", qeavbpcmsrc->stream_info.pkts_per_wake, payload_size, qeavbpcmsrc->stream_info.wakeup_period_us);

  if (qeavbpcmsrc->is_first_pcmpacket) {
    kpi_place_marker("M - qeavbpcmsrc begin recv 1st pkt.");
  }

retry:
#define PCMSRC_RETRY_KPILOG_SUPPRESS 50  //Every PCMSRC_RETRY_KPILOG_SUPPRESS, will print one KPI log
  g_mutex_lock(&qeavbpcmsrc->lock);
  if (qeavbpcmsrc->started) {
    memset(&qavb_buffer, 0, sizeof(eavb_ioctl_buf_data_t));
    qavb_buffer.hdr.payload_size = payload_size;
    qavb_buffer.pbuf = (uint64_t)qeavbpcmsrc->eavb_addr;
    GST_LOG_OBJECT (qeavbpcmsrc, "will call qeavb_receive_data(), retry %d", retry_time);
    if (qeavbpcmsrc->is_first_pcmpacket && 0 == retry_time%PCMSRC_RETRY_KPILOG_SUPPRESS) {
      snprintf(tips, sizeof(tips), "M - qeavbpcmsrc recving 1st pkt, retry %d.", retry_time);
      kpi_place_marker(tips);
    }
    recv_len = qeavb_receive_data(qeavbpcmsrc->eavb_fd, &(qeavbpcmsrc->hdr), &qavb_buffer);
    if (qeavbpcmsrc->is_first_pcmpacket && (0==retry_time%PCMSRC_RETRY_KPILOG_SUPPRESS || recv_len>0)) {
      snprintf(tips, sizeof(tips), "M - qeavbpcmsrc recv 1st pkt rt %d retry %d.", recv_len, retry_time);
      kpi_place_marker(tips);
    }
    GST_LOG_OBJECT (qeavbpcmsrc, "receive data len %d", recv_len);
    if (recv_len > 0) {
      if (qeavbpcmsrc->is_first_pcmpacket) {
        kpi_place_marker("M - qeavbpcmsrc receive the first packet");
        GST_INFO_OBJECT(qeavbpcmsrc,"receive the first pcm packet");
        qeavbpcmsrc->is_first_pcmpacket = FALSE;
      }
      gst_buffer_fill (buffer, 0, qavb_buffer.pbuf, recv_len);
      gst_buffer_set_size (buffer, recv_len);
      err = qeavb_receive_done(qeavbpcmsrc->eavb_fd, &(qeavbpcmsrc->hdr), &qavb_buffer);
      if (0 != err) {
        GST_ERROR_OBJECT (qeavbpcmsrc,"receive data (len %d) done error %d, exit!", recv_len, err);
        g_warn_if_fail(FALSE && "qeavb_receive_done() ret err after qeavb_receive_data() received data!");
        error = GST_FLOW_ERROR;
        goto finish_handle;
      }
    } else {
      err = qeavb_receive_done(qeavbpcmsrc->eavb_fd, &(qeavbpcmsrc->hdr), &qavb_buffer);
      if (0 != err) {
        GST_ERROR_OBJECT (qeavbpcmsrc,"receive data done error %d, exit!", err);
        g_warn_if_fail(FALSE && "qeavb_receive_done() ret err after qeavb_receive_data() not received data!");
        error = GST_FLOW_ERROR;
        goto finish_handle;
      }
      if (retry_time < RETRY_COUNT){
        retry_time ++;
        g_mutex_unlock(&qeavbpcmsrc->lock);
        GST_LOG_OBJECT (qeavbpcmsrc,"retry to receive data %d, will sleep time %d us\n", retry_time, sleep_us);
        g_usleep(sleep_us);
        goto retry;
      } else {
        kpi_place_marker("E - qeavbpcmsrc recv data timeout!");
        GST_ERROR_OBJECT (qeavbpcmsrc, "Failed to receive audio pcm, timeout %dus X %d", sleep_us, retry_time);
        g_warn_if_fail(FALSE && "qeavb_receive_data() receiving data timeout!");
        error = GST_FLOW_ERROR;
      }
    }
  }
finish_handle:
  g_mutex_unlock(&qeavbpcmsrc->lock);
  return error;
}

gboolean
gst_qeavb_pcm_src_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (qeavbpcmsrc_debug, "qeavbpcmsrc", 0, "QEAVB_PCM Source");
  return gst_element_register (plugin, "qeavbpcmsrc", GST_RANK_NONE,
      GST_TYPE_QEAVB_PCM_SRC);
}
