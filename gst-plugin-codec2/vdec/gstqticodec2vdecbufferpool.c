/*
* Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted (subject to the limitations in the
* disclaimer below) provided that the following conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*
*     * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
* GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
* HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
* IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <gst/gst.h>
#include "gstqticodec2vdec.h"
#include "gstqticodec2vdecbufferpool.h"
#include <media/msm_media_info.h>
#include "codec2wrapper.h"

GST_DEBUG_CATEGORY_STATIC (qtivdecbufferpool_debug);
#define GST_CAT_DEFAULT qtivdecbufferpool_debug

G_DEFINE_TYPE (Gstqticodec2vdecBufferPool, gst_qticodec2vdec_buffer_pool,
    GST_TYPE_BUFFER_POOL);

/* Function will be named qticodec2vdecbufferpool_qdata_quark() */
static G_DEFINE_QUARK (QtiCodec2BufferPoolQuark, qticodec2vdecbufferpool_qdata);

static void
print_gst_buf (gpointer key, gpointer value, gpointer data)
{
  GST_DEBUG ("key:0x%lx value:%p", *(gint64 *) key, value);
}

static void
gst_qticodec2vdec_buffer_pool_init (Gstqticodec2vdecBufferPool * pool)
{
  GST_DEBUG_CATEGORY_INIT (qtivdecbufferpool_debug,
      "qticodec2vdecpool", 0, "QTI GST codec2.0 decoder buffer pool");

  pool->buffer_table = NULL;
  pool->allocator = NULL;
}

static void
gst_qticodec2vdec_buffer_pool_finalize (GObject * obj)
{
  Gstqticodec2vdecBufferPool *pool = GST_QTICODEC2VDEC_BUFFER_POOL_CAST (obj);

  GST_DEBUG_OBJECT (pool, "finalize buffer pool:%p", pool);

  if (pool->buffer_table) {
    g_hash_table_foreach (pool->buffer_table, print_gst_buf, NULL);
    g_hash_table_destroy (pool->buffer_table);
  }

  if (pool->allocator) {
    GST_DEBUG_OBJECT (pool, "finalize allocator:%p ref cnt:%d", pool->allocator,
        GST_OBJECT_REFCOUNT (pool->allocator));
    gst_object_unref (pool->allocator);
  }

  G_OBJECT_CLASS (gst_qticodec2vdec_buffer_pool_parent_class)->finalize (obj);
}

static gboolean
mark_meta_data_pooled (GstBuffer * buffer, GstMeta ** meta, gpointer user_data)
{
  GST_META_FLAG_SET (*meta, GST_META_FLAG_POOLED);
  GST_META_FLAG_SET (*meta, GST_META_FLAG_LOCKED);

  return TRUE;
}

static GstFlowReturn
gst_qticodec2vdec_buffer_pool_acquire_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params)
{
  GstFlowReturn result;
  GstMemory *mem;
  GstBuffer *out_buf;
  GstStructure *structure;
  GstBufferPoolAcquireParamsExt *param_ext =
      (GstBufferPoolAcquireParamsExt *) params;
  Gstqticodec2vdecBufferPool *out_port_pool =
      GST_QTICODEC2VDEC_BUFFER_POOL_CAST (pool);
  Gstqticodec2vdec *dec = out_port_pool->qticodec2vdec;
  GstVideoInfo *vinfo;
  gint64 key = ((gint64) param_ext->fd << 32) | param_ext->meta_fd;
  gint64 *buf_key = NULL;
  GValue new_index = { 0, };
  g_value_init (&new_index, G_TYPE_UINT64);

  out_buf =
      (GstBuffer *) g_hash_table_lookup (out_port_pool->buffer_table, &key);
  if (out_buf) {
    GST_DEBUG_OBJECT (pool,
        "found a gst buf:%p fd:%d meta_fd:%d idx:%lu ref_cnt:%d", out_buf,
        param_ext->fd, param_ext->meta_fd, param_ext->index,
        GST_OBJECT_REFCOUNT (out_buf));
    /*replace buffer index with current one */
    structure =
        gst_mini_object_get_qdata (GST_MINI_OBJECT (out_buf),
        qticodec2vdecbufferpool_qdata_quark ());
    if (structure) {
      g_value_set_uint64 (&new_index, param_ext->index);
      gst_structure_set_value (structure, "index", &new_index);
      GST_DEBUG_OBJECT (pool, "set index:%lu into structure", param_ext->index);
    }
  } else {
    /* If can't find related gst buffer in hash table by fd/meta_fd,
     * new a gst buffer, and attach dma info to it. Add a flag
     * GST_FD_MEMORY_FLAG_DONT_CLOSE to avoid double free issue since
     * underlying ion buffer is allocated in C2 allocator rather than
     * dmabuf allocator of GST.
     */
    out_buf = gst_buffer_new ();
    mem = gst_dmabuf_allocator_alloc_with_flags (out_port_pool->allocator,
        param_ext->fd, param_ext->size, GST_FD_MEMORY_FLAG_DONT_CLOSE);
    if (G_UNLIKELY (!mem)) {
      GST_ERROR_OBJECT (pool, "failed to allocate GstDmaMemory");
      return GST_FLOW_ERROR;
    }
    gst_buffer_append_memory (out_buf, mem);

    vinfo = &dec->output_state->info;
    gsize offset[GST_VIDEO_MAX_PLANES] = { 0, };
    gint stride[GST_VIDEO_MAX_PLANES] = { 0, };

    switch (GST_VIDEO_INFO_FORMAT (vinfo)) {
      case GST_VIDEO_FORMAT_NV12:
        if (dec->is_ubwc) {
          stride[0] = stride[1] =
              VENUS_Y_STRIDE (COLOR_FMT_NV12_UBWC,
              GST_VIDEO_INFO_WIDTH (vinfo));
          offset[0] = 0;
          offset[1] = stride[0] * VENUS_Y_SCANLINES (COLOR_FMT_NV12_UBWC,
              GST_VIDEO_INFO_HEIGHT (vinfo));
        } else {
          stride[0] = stride[1] =
              VENUS_Y_STRIDE (COLOR_FMT_NV12, GST_VIDEO_INFO_WIDTH (vinfo));
          offset[0] = 0;
          offset[1] = stride[0] * VENUS_Y_SCANLINES (COLOR_FMT_NV12,
              GST_VIDEO_INFO_HEIGHT (vinfo));
        }
        break;
      default:
        g_assert_not_reached ();
        break;
    }

    /* Add video meta data, which is needed for downstream element. */
    GST_DEBUG_OBJECT (pool,
        "attach video meta: width:%d height:%d offset:%lu %lu stride:%d %d planes:%d size:%lu gst size:%lu",
        GST_VIDEO_INFO_WIDTH (vinfo), GST_VIDEO_INFO_HEIGHT (vinfo), offset[0],
        offset[1], stride[0], stride[1], GST_VIDEO_INFO_N_PLANES (vinfo),
        GST_VIDEO_INFO_SIZE (vinfo), gst_buffer_get_size (out_buf));
    gst_buffer_add_video_meta_full (out_buf, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_INFO_FORMAT (vinfo), GST_VIDEO_INFO_WIDTH (vinfo),
        GST_VIDEO_INFO_HEIGHT (vinfo), GST_VIDEO_INFO_N_PLANES (vinfo), offset,
        stride);

    /* lock all metadata and mark as pooled, we want this to remain on the buffer */
    gst_buffer_foreach_meta (out_buf, mark_meta_data_pooled, NULL);

    buf_key = g_malloc (sizeof (gint64));
    *buf_key = key;
    g_hash_table_insert (out_port_pool->buffer_table, buf_key, out_buf);
    GST_DEBUG_OBJECT (pool,
        "add a gst buf:%p fd:%d meta_fd:%d idx:%lu ref_cnt:%d", out_buf,
        param_ext->fd, param_ext->meta_fd, param_ext->index,
        GST_OBJECT_REFCOUNT (out_buf));

    structure = gst_structure_new_empty ("BUFFER");
    g_value_set_uint64 (&new_index, param_ext->index);
    gst_structure_set_value (structure, "index", &new_index);
    gst_mini_object_set_qdata (GST_MINI_OBJECT (out_buf),
        qticodec2vdecbufferpool_qdata_quark (), structure,
        (GDestroyNotify) gst_structure_free);
  }

  *buffer = out_buf;
  result = GST_FLOW_OK;

  return result;
}

static void
gst_qticodec2vdec_buffer_pool_release_buffer (GstBufferPool * pool,
    GstBuffer * buffer)
{
  GstBufferPoolClass *bp_class =
      GST_BUFFER_POOL_CLASS (gst_qticodec2vdec_buffer_pool_parent_class);
  Gstqticodec2vdecBufferPool *out_port_pool =
      GST_QTICODEC2VDEC_BUFFER_POOL_CAST (pool);

  Gstqticodec2vdec *dec = out_port_pool->qticodec2vdec;
  guint64 index = 0;
  GstStructure *structure = (GstStructure *) gst_mini_object_get_qdata
      (GST_MINI_OBJECT (buffer), qticodec2vdecbufferpool_qdata_quark ());

  if (structure) {
    /* If buffer comes from sink, free it.
     * In fact, underlying C2Allocator don't free it rather than return it
     * to internal buffer pool for recycling.
     */
    gst_structure_get_uint64 (structure, "index", &index);

    if (dec) {
      GST_DEBUG_OBJECT (pool, "release output buffer index: %ld", index);
      if (!c2component_freeOutBuffer (dec->comp, index)) {
        GST_ERROR_OBJECT (pool, "Failed to release buffer (%lu)", index);
      }
    } else {
      GST_ERROR_OBJECT (pool, "Null Gstqticodec2vdec hanlde");
    }
  } else {
    /* If buffer don't have this quark, means it's allocated in pre-allocation stage
     * of buffer pool. But it's not used since only gst buffer allocated in
     * gst_qticodec2vdec_buffer_pool_acquire_buffer is used. Here is used for releasing preallocated
     * gst buffer to internal queue of buffer pool. As a result, it can be freed once pool destroyed.
     */
    bp_class->release_buffer (pool, buffer);
  }

  return;
}

static void
gst_qticodec2vdec_buffer_pool_class_init (Gstqticodec2vdecBufferPoolClass *
    klass)
{
  GObjectClass *gobj_class = (GObjectClass *) klass;
  GstBufferPoolClass *bp_class = (GstBufferPoolClass *) klass;

  gobj_class->finalize = gst_qticodec2vdec_buffer_pool_finalize;

  bp_class->acquire_buffer = gst_qticodec2vdec_buffer_pool_acquire_buffer;
  bp_class->release_buffer = gst_qticodec2vdec_buffer_pool_release_buffer;
}

GstBufferPool *
gst_qticodec2vdec_buffer_pool_new (Gstqticodec2vdec * qticodec2vdec,
    GstAllocator * allocator, GHashTable * buffer_table)
{
  Gstqticodec2vdecBufferPool *pool;

  g_return_val_if_fail (GST_IS_QTICODEC2VDEC (qticodec2vdec), NULL);
  pool = (Gstqticodec2vdecBufferPool *)
      g_object_new (GST_TYPE_QTICODEC2VDEC_BUFFER_POOL, NULL);
  pool->qticodec2vdec = qticodec2vdec;
  pool->allocator = allocator;
  pool->buffer_table = buffer_table;

  GST_INFO_OBJECT (pool, "new output buffer pool:%p allocator:%p table:%p",
      pool, allocator, buffer_table);

  return GST_BUFFER_POOL (pool);
}
