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

#include "gstqeavbtssrc.h"

GST_DEBUG_CATEGORY_STATIC (qeavbtssrc_debug);
#define GST_CAT_DEFAULT (qeavbtssrc_debug)

#define DEFAULT_TS_CONFIG_FILE "/etc/xdg/listenerMPEG2TS.ini"
#define DEFAULT_TSSRC_IS_LIVE FALSE
enum
{
  PROP_0,
  PROP_CONFIG_FILE,
  PROP_IS_LIVE,
};

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("video/mpegts, " "systemstream = (boolean) true ")
  );


#define gst_qeavb_ts_src_parent_class parent_class
G_DEFINE_TYPE (GstQeavbTsSrc, gst_qeavb_ts_src, GST_TYPE_PUSH_SRC);

static void gst_qeavb_ts_src_finalize (GObject * gobject);

static void gst_qeavb_ts_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_qeavb_ts_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_qeavb_ts_src_setcaps (GstBaseSrc * basesrc,
    GstCaps * caps);
static GstCaps *gst_qeavb_ts_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_qeavb_ts_src_query (GstBaseSrc * basesrc, GstQuery * query);

static gboolean gst_qeavb_ts_src_start (GstBaseSrc * basesrc);
static gboolean gst_qeavb_ts_src_stop (GstBaseSrc * basesrc);
static GstFlowReturn gst_qeavb_ts_src_fill (GstPushSrc * pushsrc, GstBuffer *
    buffer);
static GstStateChangeReturn
    gst_qeavb_ts_src_change_state (GstElement * element,
    GstStateChange transition);

static void
gst_qeavb_ts_src_class_init (GstQeavbTsSrcClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *basesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS (klass);

  object_class->finalize = gst_qeavb_ts_src_finalize;
  object_class->get_property = gst_qeavb_ts_src_get_property;
  object_class->set_property = gst_qeavb_ts_src_set_property;
  element_class->change_state = gst_qeavb_ts_src_change_state;
  gst_element_class_add_static_pad_template (element_class, &src_template);
  g_object_class_install_property (object_class, PROP_CONFIG_FILE,
      g_param_spec_string ("config-file", "Config File Name",
          "Config file name to config eavb",
          DEFAULT_TS_CONFIG_FILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (object_class, PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Whether to act as a live source", DEFAULT_TSSRC_IS_LIVE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  gst_element_class_set_static_metadata (element_class,
      "TS Transport Source",
      "Src/Network", "Receive ts from the network",
      "Lily Li <lali@codeaurora.org>");

  basesrc_class->start = GST_DEBUG_FUNCPTR (gst_qeavb_ts_src_start);
  basesrc_class->stop = GST_DEBUG_FUNCPTR (gst_qeavb_ts_src_stop);
  pushsrc_class->fill = GST_DEBUG_FUNCPTR (gst_qeavb_ts_src_fill);
  basesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_qeavb_ts_src_setcaps);
  basesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_qeavb_ts_src_fixate);
  basesrc_class->query = GST_DEBUG_FUNCPTR (gst_qeavb_ts_src_query);
}

static void
gst_qeavb_ts_src_init (GstQeavbTsSrc * qeavbtssrc)
{
  gst_base_src_set_live (GST_BASE_SRC (qeavbtssrc), DEFAULT_TSSRC_IS_LIVE);
  gst_base_src_set_format (GST_BASE_SRC (qeavbtssrc), DEFAULT_TSSRC_IS_LIVE ? GST_FORMAT_TIME : GST_FORMAT_BYTES);
  gst_base_src_set_blocksize (GST_BASE_SRC (qeavbtssrc), QEAVB_TS_DEFAULT_BLOCKSIZE);

  qeavbtssrc->config_file = g_strdup (DEFAULT_TS_CONFIG_FILE);
  qeavbtssrc->eavb_addr = NULL;
  qeavbtssrc->eavb_fd = -1;
  qeavbtssrc->is_first_tspacket = TRUE;
  qeavbtssrc->started = FALSE;
  memset(&(qeavbtssrc->hdr), 0, sizeof(eavb_ioctl_hdr_t));
  memset(&(qeavbtssrc->stream_info), 0, sizeof(eavb_ioctl_stream_info_t));
  g_mutex_init (&qeavbtssrc->lock);
  kpi_place_marker("M - qeavbtssrc init");
}

static void
gst_qeavb_ts_src_finalize (GObject * object)
{
  GstQeavbTsSrc *qeavbtssrc = GST_QEAVB_TS_SRC (object);
  g_free(qeavbtssrc->config_file);
  g_mutex_clear (&qeavbtssrc->lock);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_qeavb_ts_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstQeavbTsSrc *qeavbtssrc = GST_QEAVB_TS_SRC (object);
  gboolean is_live = FALSE;
  GST_DEBUG_OBJECT (qeavbtssrc, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_CONFIG_FILE:
      g_free (qeavbtssrc->config_file);
      qeavbtssrc->config_file = g_value_dup_string (value);
      break;
    case PROP_IS_LIVE:
      is_live = g_value_get_boolean (value);
      gst_base_src_set_live (GST_BASE_SRC (qeavbtssrc), is_live);
      gst_base_src_set_format (GST_BASE_SRC (qeavbtssrc), is_live ? GST_FORMAT_TIME : GST_FORMAT_BYTES);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_qeavb_ts_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstQeavbTsSrc *qeavbtssrc = GST_QEAVB_TS_SRC (object);

  GST_DEBUG_OBJECT (qeavbtssrc, "prop_id %u", prop_id);

  switch (prop_id) {
    case PROP_CONFIG_FILE:
      g_value_set_string (value, qeavbtssrc->config_file);
      break;
    case PROP_IS_LIVE:
      g_value_set_boolean (value, gst_base_src_is_live (GST_BASE_SRC (qeavbtssrc)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_qeavb_ts_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstQeavbTsSrc *src = GST_QEAVB_TS_SRC (bsrc);
  GstStructure *structure;
  gint channels, rate;

  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);

  return caps;
}

static gboolean
gst_qeavb_ts_src_setcaps (GstBaseSrc * basesrc, GstCaps * caps)
{
  GstQeavbTsSrc *src = GST_QEAVB_TS_SRC (basesrc);

  if (NULL != src && 0 != src->stream_info.max_buffer_size && 0 != src->stream_info.pkts_per_wake)
    gst_base_src_set_blocksize (basesrc, src->stream_info.max_buffer_size * src->stream_info.pkts_per_wake);

  return TRUE;
}

static GstStateChangeReturn
gst_qeavb_ts_src_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstQeavbTsSrc *src = GST_QEAVB_TS_SRC (element);
  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_qeavb_ts_src_stop(GST_BASE_SRC (src));
    break;
    default:
    break;
  }
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  return ret;
}
static gboolean
gst_qeavb_ts_src_query (GstBaseSrc * basesrc, GstQuery * query)
{
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    default:
      res = GST_BASE_SRC_CLASS (parent_class)->query (basesrc, query);
      break;
  }

  return res;
}


static gboolean
gst_qeavb_ts_src_start (GstBaseSrc * basesrc)
{
  int err = 0;
  GstQeavbTsSrc *qeavbtssrc = GST_QEAVB_TS_SRC (basesrc);

  GST_INFO_OBJECT(qeavbtssrc,"qeavb ts src start");
  kpi_place_marker("M - qeavbtssrc start");
  qeavbtssrc->eavb_fd = open("/dev/virt-eavb", O_RDWR);
  if (qeavbtssrc->eavb_fd < 0) {
    GST_ERROR_OBJECT (qeavbtssrc,"open eavb fd error, exit!");
    return FALSE;
  }

  err = qeavb_read_config_file(&(qeavbtssrc->cfg_data), qeavbtssrc->config_file);
  if (0 == err) {
    err = qeavb_create_stream(qeavbtssrc->eavb_fd, &(qeavbtssrc->cfg_data), &(qeavbtssrc->hdr));
  }
  else {
    err = qeavb_create_stream_remote(qeavbtssrc->eavb_fd, qeavbtssrc->config_file, &(qeavbtssrc->hdr));
  }
  if (0 != err) {
    GST_ERROR_OBJECT (qeavbtssrc,"create stream error %d, exit!", err);
    goto error_close;
  }

  err = qeavb_connect_stream(qeavbtssrc->eavb_fd, &(qeavbtssrc->hdr));
  if (0 != err) {
    GST_ERROR_OBJECT (qeavbtssrc,"connect stream error %d, exit!", err);
    goto error_destroy;
  }
  GST_DEBUG_OBJECT (qeavbtssrc,"get stream info");

  err = qeavb_get_stream_info(qeavbtssrc->eavb_fd, &(qeavbtssrc->hdr), &(qeavbtssrc->stream_info));
  if (0 != err) {
    GST_ERROR_OBJECT (qeavbtssrc,"get stream info error %d, exit!", err);
    goto error_disconnect;
  }

  // mmap
  qeavbtssrc->eavb_addr = mmap(NULL, qeavbtssrc->stream_info.max_buffer_size * qeavbtssrc->stream_info.pkts_per_wake, PROT_READ | PROT_WRITE, MAP_SHARED, qeavbtssrc->eavb_fd, 0);
  qeavbtssrc->started = TRUE;
  kpi_place_marker("M - qeavbtssrc started successful");
  GST_DEBUG_OBJECT (qeavbtssrc, "QEAVB ts source started");
  return TRUE;

error_disconnect:
  err = qeavb_disconnect_stream(qeavbtssrc->eavb_fd, &(qeavbtssrc->hdr));
    if (0 != err) {
      GST_ERROR_OBJECT (qeavbtssrc,"disconnect stream error %d!", err);
    }
error_destroy:
    GST_DEBUG_OBJECT (qeavbtssrc,"destroying stream");
    err = qeavb_destroy_stream(qeavbtssrc->eavb_fd, &(qeavbtssrc->hdr));
    if (0 != err) {
      GST_ERROR_OBJECT (qeavbtssrc,"destroy stream error %d!", err);
    }
error_close:
  if (qeavbtssrc->eavb_fd >= 0) {
    close(qeavbtssrc->eavb_fd);
    qeavbtssrc->eavb_fd = -1;
  }
  return FALSE;
}

static gboolean
gst_qeavb_ts_src_stop (GstBaseSrc * basesrc)
{
  int err = 0;
  GstQeavbTsSrc *qeavbtssrc = GST_QEAVB_TS_SRC (basesrc);

  g_mutex_lock(&qeavbtssrc->lock);
  if (qeavbtssrc->started) {
    munmap(qeavbtssrc->eavb_addr, qeavbtssrc->stream_info.max_buffer_size * qeavbtssrc->stream_info.pkts_per_wake);

    GST_DEBUG_OBJECT (qeavbtssrc,"desconnect stream");
    err = qeavb_disconnect_stream(qeavbtssrc->eavb_fd, &(qeavbtssrc->hdr));
    if (0 != err) {
      GST_ERROR_OBJECT (qeavbtssrc,"disconnect stream error %d, exit!", err);
    }

    GST_DEBUG_OBJECT (qeavbtssrc,"destroying stream");
    err = qeavb_destroy_stream(qeavbtssrc->eavb_fd, &(qeavbtssrc->hdr));
    if (0 != err) {
      GST_ERROR_OBJECT (qeavbtssrc,"destroy stream error %d, exit!", err);
    }

    close(qeavbtssrc->eavb_fd);
    qeavbtssrc->eavb_fd = -1;
    GST_DEBUG_OBJECT (qeavbtssrc, "QEAVB ts source stopped");
  }
  qeavbtssrc->started = FALSE;
  g_mutex_unlock(&qeavbtssrc->lock);
  return TRUE;
}

static GstFlowReturn
gst_qeavb_ts_src_fill (GstPushSrc * pushsrc, GstBuffer * buffer)
{
  GstMapInfo map;
  gint32 retry_time = 0;
  gint32 sleep_us = DEFALUT_SLEEP_US;
  eavb_ioctl_buf_data_t qavb_buffer;
  gint32 recv_len = 0;
  GstQeavbTsSrc *qeavbtssrc = GST_QEAVB_TS_SRC (pushsrc);
  GstFlowReturn error = GST_FLOW_OK;
  int err = 0;
  guint32 payload_size = 0;

  if (qeavbtssrc->is_first_tspacket) {
    kpi_place_marker("M - qeavbtssrc begin recv 1st pkt.");
  }

  if(qeavbtssrc->stream_info.wakeup_period_us != 0)
    sleep_us = qeavbtssrc->stream_info.wakeup_period_us;

  payload_size = qeavbtssrc->stream_info.max_buffer_size * qeavbtssrc->stream_info.pkts_per_wake;

retry:
  g_mutex_lock(&qeavbtssrc->lock);
  if (qeavbtssrc->started) {
    memset(&qavb_buffer, 0, sizeof(eavb_ioctl_buf_data_t));
    qavb_buffer.hdr.payload_size = payload_size;
    qavb_buffer.pbuf = (uint64_t)qeavbtssrc->eavb_addr;
    GST_DEBUG_OBJECT (qeavbtssrc, "pkts_per_wake %d, qavb_buffer.hdr.payload_size %d",qeavbtssrc->stream_info.pkts_per_wake, qavb_buffer.hdr.payload_size);
    recv_len = qeavb_receive_data(qeavbtssrc->eavb_fd, &(qeavbtssrc->hdr), &qavb_buffer);
    GST_DEBUG_OBJECT (qeavbtssrc, "receive data len %d", recv_len);
    if (recv_len > 0) {
      if (qeavbtssrc->is_first_tspacket) {
        kpi_place_marker("M - qeavbtssrc receive the first packet");
        GST_INFO_OBJECT(qeavbtssrc,"receive the first ts packet");
        qeavbtssrc->is_first_tspacket = FALSE;
      }
      gst_buffer_fill (buffer, 0, qavb_buffer.pbuf, recv_len);
      gst_buffer_set_size (buffer, recv_len);
      err = qeavb_receive_done(qeavbtssrc->eavb_fd, &(qeavbtssrc->hdr), &qavb_buffer);
      if (0 != err) {
        GST_ERROR_OBJECT (qeavbtssrc,"receive data (len %d) done error %d, exit!", recv_len, err);
        error = GST_FLOW_ERROR;
        goto finish_handle;
      }
    } else {
      err = qeavb_receive_done(qeavbtssrc->eavb_fd, &(qeavbtssrc->hdr), &qavb_buffer);
      if (0 != err) {
        GST_ERROR_OBJECT (qeavbtssrc,"receive data done error %d, exit!", err);
        error = GST_FLOW_ERROR;
        goto finish_handle;
      }
      if (retry_time < RETRY_COUNT){
        retry_time ++;
        g_mutex_unlock(&qeavbtssrc->lock);
        GST_DEBUG_OBJECT (qeavbtssrc,"retry to receive data %d, will sleep time %d us\n", retry_time, sleep_us);
        g_usleep(sleep_us);
        goto retry;
      } else {
        kpi_place_marker("E - qeavbtssrc recv data timeout!");
        GST_ERROR_OBJECT (qeavbtssrc, "Failed to receive ts, timeout %dus X %d", sleep_us, retry_time);
        error = GST_FLOW_ERROR;
      }
    }
  }
finish_handle:
  g_mutex_unlock(&qeavbtssrc->lock);
  return error;
}

gboolean
gst_qeavb_ts_src_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (qeavbtssrc_debug, "qeavbtssrc", 0, "QEAVB TS Source");
  return gst_element_register (plugin, "qeavbtssrc", GST_RANK_NONE,
      GST_TYPE_QEAVB_TS_SRC);
}
