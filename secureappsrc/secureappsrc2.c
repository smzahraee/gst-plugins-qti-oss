/*-------------------------------------------------------------------
Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--------------------------------------------------------------------*/

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/msm_ion.h>
#include <ion/ion.h>
#include <sys/mman.h>
#include <linux/dma-buf.h>

#define SECURE_PLAYBACK
#include "crypto.h"

FILE *fp = NULL;

typedef struct _secureappsrc {
  GMainLoop *loop;
  GQueue *sec_buf_queue;
  Crypto *crypto;
  GMutex file_lock;
  GMutex buf_lock;
  GCond buf_cond;
  void *sec_buf_addr;
} secureappsrc;

static void onNewPad(GstElement *decodebin, GstPad *pad, GstElement *userData)
{
  GstPad *newpad;
  newpad = gst_element_get_static_pad(GST_ELEMENT(userData), "sink");
  gst_pad_link(pad, newpad);
  g_object_unref(newpad);
}

static void onOMXVideoDecCreated (GstBin * bin, GstBin * sub_bin,
    GstElement * element, gpointer user_data)
{
#ifdef SECURE_PLAYBACK
  // for the secure playback, make sure the following properties have been set
  g_object_set (element, "secure", 1, NULL);
#endif
  g_object_set (element, "input-buffer-sharing", 1, NULL);
}

/* The function is triggered when element GST_APPSRC queue is empty. It requires more buffers to
   fill. Firstly, it reads raw video data, then copies the data to input buffer of OMX input
   port. For simplicity, there are a four bytes header before every frame data. The four bytes
   are size of the following frame data.
*/
static void onNeedData(GstElement *appsrc, guint dataSize, secureappsrc *secureappsrc)
{
  GstBuffer *buffer;
  int length = 0;
  int len = 0;
  OMX_BUFFERHEADERTYPE *free_sec_ion_buf = NULL;
  g_mutex_lock (&secureappsrc->file_lock);
  for (int i=0; i<4; i++) {
    len = 0;
    fread(&len, 1, 1, fp);
    length += ( len<< (8*i));
  }

RETRY:
  g_mutex_lock (&secureappsrc->buf_lock);
  free_sec_ion_buf = (OMX_BUFFERHEADERTYPE *)g_queue_pop_head (secureappsrc->sec_buf_queue);
  if (free_sec_ion_buf == NULL) {
    GST_DEBUG ("no empty input secure buffer");
    g_cond_wait (&secureappsrc->buf_cond, &secureappsrc->buf_lock);
    g_mutex_unlock (&secureappsrc->buf_lock);
    GST_DEBUG ("cond waited");
    goto RETRY;
  } else {

  }
  g_mutex_unlock (&secureappsrc->buf_lock);

  int ret;

  unsigned char *data = g_malloc0 (length);
  free_sec_ion_buf->nFilledLen = length;


  GST_DEBUG ("sec2 etb:%p size:%d sec ion nAllocLen:%d fd:%d len:%d\n",
    free_sec_ion_buf, free_sec_ion_buf->nFilledLen, free_sec_ion_buf->nAllocLen, free_sec_ion_buf->pBuffer, length);

  ret = fread(data, 1, length, fp);

  g_mutex_unlock (&secureappsrc->file_lock);

#ifdef SECURE_PLAYBACK
  if (ret > 0) {
    SecureCopyResult ret1 = crypto_copy (secureappsrc->crypto, SECURE_COPY_NONSECURE_TO_SECURE,
        data, (unsigned long)free_sec_ion_buf->pBuffer, length);
    g_free (data);
    if (ret1 != SECURE_COPY_SUCCESS) {
      GST_ERROR ("copy non-secure buf to secure buf failed");
    }
  }
#else
  char *bufaddr = (char*)mmap(NULL, free_sec_ion_buf->nAllocLen, PROT_READ|PROT_WRITE, MAP_SHARED, (gint64)free_sec_ion_buf->pBuffer, 0);
  if (bufaddr == MAP_FAILED) {
    GST_ERROR ("mmap failed");
  } else {
    memcpy (bufaddr, data, length);
    g_free (data);
  }
#endif


  if (ret > 0)
  {
    GstBuffer *buffer;
    GST_DEBUG ("sec1 etb:%p size:%d sec ion nAllocLen:%d fd:%d len:%d\n",
      free_sec_ion_buf, free_sec_ion_buf->nFilledLen, free_sec_ion_buf->nAllocLen, free_sec_ion_buf->pBuffer, length);
    buffer = gst_buffer_new_allocate(NULL, sizeof(OMX_BUFFERHEADERTYPE*), NULL);
    gst_buffer_fill (buffer, 0, &free_sec_ion_buf, sizeof(OMX_BUFFERHEADERTYPE*));

    gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);
  }
  else
  {
    gst_app_src_end_of_stream(GST_APP_SRC(appsrc));
    GST_DEBUG ("sent eos");
  }
}

static gboolean msg_handler(GstBus *bus, GstMessage *msg, gpointer data)
{
  secureappsrc *appsrc = (secureappsrc *)data;
  GMainLoop *loop = appsrc->loop;
  OMX_BUFFERHEADERTYPE *omx_buf_header = NULL;

  if (msg->type == GST_MESSAGE_EOS) {
    g_main_loop_quit (loop);
    GST_DEBUG ("the pipeline will be ended because of the EOS message");
  } else if (msg->type == GST_MESSAGE_ERROR) {
    g_main_loop_quit (loop);
    GST_ERROR ("the pipiple post a error");
  } else {
    if (gst_message_has_name (msg, "omx-dec-buf-fd")) {
      const GstStructure *s = gst_message_get_structure (msg);
      gst_structure_get (s, "buf-fd", G_TYPE_POINTER, &omx_buf_header, NULL);
      GST_DEBUG ("buf:%p fd:%d freed\n", omx_buf_header, omx_buf_header->pBuffer);
      g_mutex_lock (&appsrc->buf_lock);
      if (omx_buf_header) {
        g_queue_push_tail (appsrc->sec_buf_queue, omx_buf_header);
        GST_DEBUG ("add buf to queue");
      } else {
        GST_ERROR ("error buf fd:%p", omx_buf_header);
        g_main_loop_quit(loop);
      }
      g_cond_broadcast (&appsrc->buf_cond);
      g_mutex_unlock (&appsrc->buf_lock);
    } else {
      GST_DEBUG ("didn't handle msg type:%d", msg->type);
    }
  }

  return TRUE;
}

int main(int argc, char **argv)
{
  gst_init(NULL, NULL);
  GMainLoop *loop;
  GstElement *appsrc;
  GstElement *waylandsink;
  GstElement *decodebin;
  GstElement *pipeline;
  int code_type = 0;
  char *stream_file;
  char *in_caps;
  guint bus_watch_id;

  if (argc != 3) {
    GST_ERROR (" error input argument passed, e.g. ./secureappsrc "
        "[codecid: h264:1;h265:2;vp9:3;mpeg2:4] [stream file]");
    return 0;
  } else {
    code_type = atoi(argv[1]);
    stream_file = argv[2];
    if (code_type > 4 || code_type < 1 || stream_file == NULL) {
      GST_ERROR (" code_type or stream file input error");
      return 0;
    } else {
      switch (code_type) {
        case 1:
#ifdef SECURE_PLAYBACK
          in_caps = "video/x-h264secure";
#else
          in_caps = "video/x-h264, stream-format=(string)byte-stream, alignment=(string)au, level=(string)4, profile=(string)high, width=(int)1920, height=(int)1080, pixel-aspect-ratio=(fraction)1/1, interlace-mode=(string)progressive, chroma-format=(string)4:2:0, bit-depth-luma=(uint)8, bit-depth-chroma=(uint)8, parsed=(boolean)true";
#endif
          break;
        case 2:
        in_caps = "video/x-h265secure";
        break;
        case 3:
        in_caps = "video/x-vp9secure";
        break;
        case 4:
        in_caps = "video/x-mpeg2secure";
        break;
      }
    }
  }
  fp = fopen( stream_file , "r" );
  secureappsrc *appsrc_struct = g_new0(secureappsrc, 1);
  g_mutex_init (&appsrc_struct->file_lock);
  g_mutex_init (&appsrc_struct->buf_lock);
  g_cond_init (&appsrc_struct->buf_cond);
  appsrc_struct->sec_buf_queue = g_queue_new ();

#ifdef SECURE_PLAYBACK
  appsrc_struct->crypto = (Crypto*)g_new0(Crypto, 1);
  crypto_init (appsrc_struct->crypto);
#endif

  gboolean rett = FALSE;

  loop = g_main_loop_new(NULL, FALSE);
  appsrc_struct->loop = loop;
  appsrc = gst_element_factory_make("appsrc", "appsrc");
  GstCaps *caps;

  caps = gst_caps_from_string (in_caps);
  g_object_set (appsrc, "caps", caps, NULL);
  gst_caps_unref (caps);
  waylandsink = gst_element_factory_make("waylandsink", "waylandsink");
#ifdef SECURE_PLAYBACK
  decodebin = gst_element_factory_make("decodebin", "decodebin");
#else
  decodebin = gst_element_factory_make("omxh264dec", "omxh264dec");
  g_object_set (decodebin, "input-buffer-sharing", 1, NULL);
#endif
  pipeline = gst_pipeline_new("pipeline");

  gst_bin_add_many(GST_BIN(pipeline), appsrc, decodebin, waylandsink, NULL);
#ifdef SECURE_PLAYBACK
  gst_element_link_many(appsrc, decodebin, NULL);
#else
  gst_element_link_many(appsrc, decodebin, waylandsink, NULL);
#endif

#ifdef SECURE_PLAYBACK
  g_signal_connect(G_OBJECT(decodebin), "pad-added", G_CALLBACK(onNewPad), waylandsink);
  g_signal_connect(GST_BIN(decodebin), "deep-element-added", G_CALLBACK(onOMXVideoDecCreated), appsrc_struct);
#endif

  g_signal_connect(G_OBJECT(appsrc), "need-data", G_CALLBACK(onNeedData), appsrc_struct);

  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  bus_watch_id = gst_bus_add_watch (bus, msg_handler, appsrc_struct);
  gst_object_unref(bus);


  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  GST_DEBUG ("pipeline set playing");
  g_main_loop_run(loop);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  GST_DEBUG ("pipeline set null");
  g_source_remove (bus_watch_id);

  g_main_loop_unref(loop);
  g_mutex_clear (&appsrc_struct->file_lock);
  g_mutex_clear (&appsrc_struct->buf_lock);
  g_cond_clear (&appsrc_struct->buf_cond);
  g_queue_free (appsrc_struct->sec_buf_queue);
#ifdef SECURE_PLAYBACK
  crypto_deinit (appsrc_struct->crypto);
  g_free (appsrc_struct->crypto);
#endif
  g_free(appsrc_struct);

  if (fp) {
    fclose(fp);
    fp = NULL;
  }
  return 0;
}
