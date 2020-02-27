/*-------------------------------------------------------------------
Copyright (c) 2020, The Linux Foundation. All rights reserved.

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

#include <linux/msm_ion.h>
#include <ion/ion.h>
#include <sys/mman.h>
#include <linux/dma-buf.h>

#define SECURE_PLAYBACK
#include "crypto.h"

FILE *fp = NULL;
int buf_size = 16588800;
// the input size is very important, don't change to another size

#define SEC_ION_BUF_CAPACITY	6
typedef struct _SecureBitsIonBuf {
  int dev_fd;
  struct ion_allocation_data alloc_data;
  int data_fd;
  unsigned char* buf_cpuaddr;
  int size;
  int used;
  int buf_idx;
} SecureBitsIonBuf;

typedef struct _secureappsrc {
  GMainLoop *loop;
  SecureBitsIonBuf *sec_buf;
  Crypto *crypto;
  GMutex file_lock;
  GMutex buf_lock;
  GCond buf_cond;
  void *sec_buf_addr;
} secureappsrc;

void do_cache_operations(int fd)
{
  if (fd < 0)
    return;

  struct dma_buf_sync dma_buf_sync_data[2];
  dma_buf_sync_data[0].flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
  dma_buf_sync_data[1].flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;

  for(unsigned int i=0; i<2; i++) {
    int rc = ioctl(fd, DMA_BUF_IOCTL_SYNC, &dma_buf_sync_data[i]);
    if (rc < 0) {
      GST_ERROR("Failed DMA_BUF_IOCTL_SYNC %s fd : %d", i==0?"start":"end", fd);
      return;
    }
  }

}

gboolean alloc_map_ion_memory(int buffer_size, SecureBitsIonBuf *ion_info, int flag)
{
  int rc = -EINVAL;
  int ion_dev_flag;
  gboolean secure_mode = TRUE;

  if (!ion_info || buffer_size <= 0) {
    GST_ERROR("Invalid arguments to alloc_map_ion_memory");
    return FALSE;
  }

  ion_info->dev_fd = ion_open();
  if (ion_info->dev_fd < 0) {
    GST_ERROR("opening ion device failed with ion_fd = %d", ion_info->dev_fd);
    return FALSE;
  }

  ion_info->alloc_data.flags = flag;
  ion_info->alloc_data.len = buffer_size;

  ion_info->alloc_data.heap_id_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);
  if (secure_mode && (ion_info->alloc_data.flags & ION_FLAG_SECURE)) {
    // FIXME: ION_SECURE_HEAP_ID
    ion_info->alloc_data.heap_id_mask = ION_HEAP(ION_SECURE_HEAP_ID);
    GST_ERROR(" secure heap id");
  }

  /* Use secure display cma heap for obvious reasons. */
  if (ion_info->alloc_data.flags & ION_FLAG_CP_BITSTREAM) {
    ion_info->alloc_data.heap_id_mask |= ION_HEAP(ION_SECURE_DISPLAY_HEAP_ID);
  }


  GST_ERROR(" ion fd:%d flags:0x%x len:%d mask:0x%x", ion_info->dev_fd,ion_info->alloc_data.flags,ion_info->alloc_data.len,ion_info->alloc_data.heap_id_mask );
  rc = ion_alloc_fd(ion_info->dev_fd, ion_info->alloc_data.len, 0,
      ion_info->alloc_data.heap_id_mask, ion_info->alloc_data.flags,
      &ion_info->data_fd);

  if (rc || ion_info->data_fd < 0) {
    GST_ERROR("ION ALLOC memory failed rc:%d", rc);
    ion_close(ion_info->dev_fd);
    ion_info->data_fd = -1;
    ion_info->dev_fd = -1;
    return FALSE;
  }

  GST_ERROR("Alloc ion memory: fd (dev:%d data:%d) len %d flags %#x mask %#x",
      ion_info->dev_fd, ion_info->data_fd, (unsigned int)ion_info->alloc_data.len,
      (unsigned int)ion_info->alloc_data.flags,
      (unsigned int)ion_info->alloc_data.heap_id_mask);

  return TRUE;
}

void free_ion_memory(SecureBitsIonBuf *buf_ion_info)
{
  if (!buf_ion_info) {
    GST_ERROR("ION: free called with invalid fd/allocdata");
    return;
  }
  GST_ERROR("Free ion memory: mmap fd %d ion_dev fd %d len %d flags %#x mask %#x",
      buf_ion_info->data_fd, buf_ion_info->dev_fd,
      (unsigned int)buf_ion_info->alloc_data.len,
      (unsigned int)buf_ion_info->alloc_data.flags,
      (unsigned int)buf_ion_info->alloc_data.heap_id_mask);

  if (buf_ion_info->data_fd >= 0) {
    close(buf_ion_info->data_fd);
    buf_ion_info->data_fd = -1;
  }
  if (buf_ion_info->dev_fd >= 0) {
    ion_close(buf_ion_info->dev_fd);
    buf_ion_info->dev_fd = -1;
  }
}

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
  g_object_set (element, "dynamic-input-buffer-mode", 1, NULL);
#endif
}

// the appsrc queue is empty, need more buffers
static void onNeedData(GstElement *appsrc, guint dataSize, secureappsrc *secureappsrc)
{
  //FIXME free data
  int length = 0;
  int len = 0;
  g_mutex_lock (&secureappsrc->file_lock);
  for (int i=0; i<4; i++) {
    len = 0;
    fread(&len, 1, 1, fp);
    length += ( len<< (8*i));
  }
  SecureBitsIonBuf *free_sec_ion_buf;
  int i;

RETRY:
  g_mutex_lock (&secureappsrc->buf_lock);
  for (i=0; i<SEC_ION_BUF_CAPACITY; i++) {
    if (!secureappsrc->sec_buf[i].used) {
      free_sec_ion_buf = secureappsrc->sec_buf + i;
      free_sec_ion_buf->used = TRUE;
      break;
    }
  }
  g_mutex_unlock (&secureappsrc->buf_lock);

  if (i == SEC_ION_BUF_CAPACITY) {
    GST_ERROR("[WARNING]  no empty input secure buffer");
    g_mutex_lock (&secureappsrc->buf_lock);
    g_cond_wait (&secureappsrc->buf_cond, &secureappsrc->buf_lock);
    g_mutex_unlock (&secureappsrc->buf_lock);
    GST_ERROR(" cond waited");
    goto RETRY;
  }


  g_mutex_lock (&secureappsrc->buf_lock);
  int ret;
#ifndef SECURE_PLAYBACK
  unsigned char *data = free_sec_ion_buf->buf_cpuaddr;
  free_sec_ion_buf->size = length;
  ret = fread(data, 1, length, fp);
#else
  unsigned char *data = g_malloc0 (length);
  free_sec_ion_buf->size = length;
  GST_ERROR(" virtal addr:0x%x buf idx:%d id:%d sizeof:%d length:%d", data, i, free_sec_ion_buf->buf_idx, sizeof(SecureBitsIonBuf), length);
  ret = fread(data, 1, length, fp);
#endif
  g_mutex_unlock (&secureappsrc->buf_lock);

#ifdef SECURE_PLAYBACK
  if (ret > 0) {
    SecureCopyResult ret1 = crypto_copy (secureappsrc->crypto, SECURE_COPY_NONSECURE_TO_SECURE,
        data, free_sec_ion_buf->data_fd, length);

    g_free (data);
    if (ret1 != SECURE_COPY_SUCCESS) {
      GST_ERROR("ccc copy non-secure buf to secure buf failed");
    }
  }
#endif

  g_mutex_unlock (&secureappsrc->file_lock);

  if (ret > 0)
  {
    g_mutex_lock (&secureappsrc->buf_lock);
    GstBuffer *buffer;
    buffer = gst_buffer_new_allocate(NULL, sizeof(SecureBitsIonBuf), NULL);
    GstMapInfo in_info;
    gst_buffer_map(buffer, &in_info, GST_MAP_WRITE);
    memcpy (in_info.data, free_sec_ion_buf ,sizeof(SecureBitsIonBuf));

    gst_buffer_unmap(buffer, &in_info);
    gst_app_src_push_buffer(appsrc, buffer);
    g_mutex_unlock (&secureappsrc->buf_lock);
  }
  else
  {
    gst_app_src_end_of_stream(appsrc);
  }
}

static gboolean msg_handler(GstBus *bus, GstMessage *msg, gpointer data)
{
  secureappsrc *appsrc = (secureappsrc *)data;
  GMainLoop *loop = appsrc->loop;
  gint buf_idx;
  if (msg->type == GST_MESSAGE_EOS) {
    g_main_loop_quit (loop);
    GST_DEBUG ("the pipeline will be ended because of the EOS message");
  } else if (msg->type == GST_MESSAGE_ERROR) {
    g_main_loop_quit (loop);
    GST_ERROR ("the pipiple post a error");
  } else {
    if (gst_message_has_name (msg, "omx-video-sec-etbd")) {
      const GstStructure *s = gst_message_get_structure (msg);
      gst_structure_get (s,
          "buf-idx", G_TYPE_INT, &buf_idx, NULL);
      GST_ERROR(" buf index:%d freed\n", buf_idx);
      g_mutex_lock (&appsrc->buf_lock);
      if (buf_idx < SEC_ION_BUF_CAPACITY) {
        appsrc->sec_buf[buf_idx].used = FALSE;
      } else {
        GST_ERROR(" error buf idx:%d", buf_idx);
        g_main_loop_quit(loop);
      }

      g_cond_broadcast (&appsrc->buf_cond);
      g_mutex_unlock (&appsrc->buf_lock);
    } else {
      GST_DEBUG("didn't handle msg type:%d", msg->type);
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
    GST_ERROR(" error input argument passed, e.g. ./secureappsrc "
        "[codecid: h264:1;h265:2;vp9:3;mpeg2:4] [stream file]");
    return 0;
  } else {
    code_type = atoi(argv[1]);
    stream_file = argv[2];
    if (code_type > 4 || code_type < 1 || stream_file == NULL) {
      GST_ERROR(" code_type or stream file input error");
      return 0;
    } else {
      switch (code_type) {
        case 1:
#ifdef SECURE_PLAYBACK
          in_caps = "video/x-h264secure";
#else
          in_caps = "video/x-h264, stream-format=(string)byte-stream, alignment=(string)au, level=(string)4, profile=(string)high, width=(int)1920, height=(int)1080, framerate=(fraction)25/1, pixel-aspect-ratio=(fraction)1/1, interlace-mode=(string)progressive, chroma-format=(string)4:2:0, bit-depth-luma=(uint)8, bit-depth-chroma=(uint)8, parsed=(boolean)true";
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

  SecureBitsIonBuf *st_vdec_ion = g_new0 (SecureBitsIonBuf, SEC_ION_BUF_CAPACITY);
#ifdef SECURE_PLAYBACK
  appsrc_struct->crypto = (Crypto*)g_new0(Crypto, 1);
  crypto_init (appsrc_struct->crypto);
#endif

  gboolean rett = FALSE;
  for (int i=0; i<SEC_ION_BUF_CAPACITY; i++) {
#ifdef SECURE_PLAYBACK
    rett = alloc_map_ion_memory (buf_size, st_vdec_ion+i, (ION_FLAG_SECURE|ION_FLAG_CP_BITSTREAM));
#else
    rett = alloc_map_ion_memory (buf_size, st_vdec_ion+i, ION_FLAG_CACHED);
#endif
    if (rett) {
      GST_ERROR(" allc ion memory successful id:%d", i);

      (st_vdec_ion+i)->buf_idx = i;
      appsrc_struct->sec_buf = st_vdec_ion;
#ifndef SECURE_PLAYBACK
      void *data = (void *) mmap (NULL, buf_size, PROT_WRITE, MAP_SHARED, st_vdec_ion[i].data_fd, 0);
      if (data == NULL) {
        GST_ERROR(" mmap failed");
      } else {
        do_cache_operations (st_vdec_ion[i].data_fd);
        appsrc_struct->sec_buf[i].buf_cpuaddr = data;
        GST_ERROR(" virtal addr:0x%x", data);
      }
#endif

    } else {
      GST_ERROR(" allc ion memory failed id:%d", i);
    }
  }

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
  g_object_set (decodebin, "dynamic-input-buffer-mode", 1, NULL);
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
  GST_ERROR(" set playing");
  g_main_loop_run(loop);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  g_source_remove (bus_watch_id);

  g_main_loop_unref(loop);
  g_mutex_clear (&appsrc_struct->file_lock);
  g_mutex_clear (&appsrc_struct->buf_lock);
  g_cond_clear (&appsrc_struct->buf_cond);
#ifdef SECURE_PLAYBACK
  crypto_deinit (appsrc_struct->crypto);
  g_free (appsrc_struct->crypto);
#endif
  g_free(appsrc_struct);


  for (int i=0; i<SEC_ION_BUF_CAPACITY; i++) {
#ifndef SECURE_PLAYBACK
    do_cache_operations ((st_vdec_ion+i)->data_fd);
    munmap ((st_vdec_ion+i)->buf_cpuaddr, buf_size);
#endif
    free_ion_memory (st_vdec_ion+i);
  }
  g_free (st_vdec_ion);
  fclose(fp);
  return 0;
}
