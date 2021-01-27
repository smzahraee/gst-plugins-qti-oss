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
#include "config.h"
#endif

#include "gst/gstinfo.h"
#include "gst/gstbufferpool.h"
#include "gstqticodec2bufferpool.h"
#include <media/msm_media_info.h>

GST_DEBUG_CATEGORY_STATIC (gst_qticodec2_debug);
#define GST_CAT_DEFAULT gst_qticodec2_debug

G_DEFINE_TYPE (GstQticodec2Allocator, gst_qticodec2_allocator,
    GST_TYPE_FD_ALLOCATOR);

static void
gst_qticodec2_allocator_init (GstQticodec2Allocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);
  alloc->mem_type = "QtiCodec2Allocator";
}

GstAllocator *
gst_qticodec2_allocator_new (gpointer comp, BUFFER_POOL_TYPE pool_type, GstCaps * caps)
{
  GstQticodec2Allocator *c2_allocator;

  c2_allocator = g_object_new (GST_TYPE_QTICODEC2_ALLOCATOR, NULL);

  c2_allocator->comp = comp;
  c2_allocator->pool_type = pool_type;

  c2_allocator->info = g_slice_new (GstVideoInfo);

  if (!gst_video_info_from_caps (c2_allocator->info, caps)) {
    GST_ERROR_OBJECT (c2_allocator, "failed to get video info");
  }

  return c2_allocator;
}

static void
gst_qticodec2_allocator_dispose (GObject * obj)
{
  GstQticodec2Allocator *c2_allocator = GST_QTICODEC2_ALLOCATOR_CAST (obj);

  G_OBJECT_CLASS (gst_qticodec2_allocator_parent_class)->dispose (obj);
}

static GstMemory *
gst_qticodec2_allocator_alloc (GstAllocator * alloc, gsize size,
    GstAllocationParams * params)
{
  GstQticodec2Allocator *c2_allocator;
  GstMemory *mem = NULL;
  GstVideoInfo *info = NULL;
  GstVideoFormat format;
  guint32 width;
  guint32 height;
  BufferDescriptor buffer;
  BUFFER_POOL_TYPE poolType;

  c2_allocator = GST_QTICODEC2_ALLOCATOR_CAST (alloc);
  info = c2_allocator->info;

  format = GST_VIDEO_FORMAT_INFO_FORMAT(info->finfo);
  width = info->width;
  height = info->height;

  /* Note: size is not used here for graphic buffer */
  GST_DEBUG_OBJECT (c2_allocator, "Allocating buffer size: %lu, format: %s, width: %d, height: %d",
      size, gst_video_format_to_string (format), width, height);

  /*TODO: add support for Linear buffer */
  if (format == GST_VIDEO_FORMAT_NV12 || format == GST_VIDEO_FORMAT_NV12_UBWC) {
      poolType = BUFFER_POOL_BASIC_GRAPHIC;

      if(!c2component_alloc (c2_allocator->comp, &buffer, poolType)) {
        GST_ERROR_OBJECT (c2_allocator, "Failed to allocate graphic buffer");
      } else {
        GST_DEBUG_OBJECT (c2_allocator, "Allocated buffer fd: %d, size: %d",
            buffer.fd, buffer.capacity);

        /* use GstFdAllocator to allocate GBM based fd memory */
        mem = gst_fd_allocator_alloc (alloc, buffer.fd, buffer.capacity, GST_FD_MEMORY_FLAG_NONE);
      }
  }

  return mem;
}

static void
gst_qticodec2_allocator_class_init (GstQticodec2AllocatorClass * klass)
{
}

#define gst_qticodec2_buffer_pool_parent_class parent_class
G_DEFINE_TYPE (GstQticodec2BufferPool, gst_qticodec2_buffer_pool,
    GST_TYPE_BUFFER_POOL);

static const char **
gst_qticodec2_buffer_pool_get_options (GstBufferPool * pool)
{
  static const gchar *options[] = { GST_BUFFER_POOL_OPTION_VIDEO_META,
    GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT, NULL};

  return options;
}

static gboolean
gst_qticodec2_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config)
{
  GstCaps *caps;
  guint32 size, min, max;
  GstQticodec2BufferPool *self_pool = GST_QTICODEC2_BUFFER_POOL_CAST (pool);
  GstQticodec2Allocator *c2_allocator = GST_QTICODEC2_ALLOCATOR_CAST(self_pool->c2_allocator);
  GstVideoInfo *info = c2_allocator->info;

  if (config) {
    if (!gst_buffer_pool_config_get_params (config, &caps, &size, &min, &max)) {
      GST_WARNING_OBJECT (pool, "invalid config");
      return FALSE;
    }
  }

  if (NULL == caps) {
    GST_INFO_OBJECT (pool, "no caps in config, ignore this config");
    return FALSE;
  } else {
    if (!gst_video_info_from_caps (info, caps)) {
      GST_ERROR_OBJECT (pool, "failed to get video info");
      return FALSE;
    }

    GST_INFO_OBJECT (pool, "%dx%d, caps %" GST_PTR_FORMAT ", format = %s",
        GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info), caps,
        gst_video_format_to_string (info->finfo->format));
  }

  if (size)
    c2_allocator->alloc_size = size;

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (pool, config);
}

static void
gst_qticodec2_buffer_pool_init (GstQticodec2BufferPool * pool)
{
  GST_DEBUG_CATEGORY_INIT (gst_qticodec2_debug,
      "qticodec2", 0, "QTI GST codec2.0");

  GST_DEBUG_OBJECT (pool, "QTI Codec2 pool init");
}

static GstFlowReturn
gst_qticodec2_buffer_pool_alloc (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params)
{
  GstQticodec2BufferPool *self_pool;
  GstQticodec2Allocator *allocator;
  GstVideoInfo *info;
  GstBuffer *buf;
  GstMemory *mem;
  gsize size;

  GST_DEBUG_OBJECT (pool, "alloc buffer");

  self_pool = GST_QTICODEC2_BUFFER_POOL_CAST (pool);
  allocator = GST_QTICODEC2_ALLOCATOR_CAST (self_pool->c2_allocator);
  size = allocator->alloc_size;

  mem = gst_qticodec2_allocator_alloc (allocator, size, NULL);

  /* create a gst buffer */
  buf = gst_buffer_new ();

  /* insert fd memmory into the gstbuffer */
  gst_buffer_prepend_memory (buf, mem);

  if (buf == NULL)
    goto no_buf;
  else
    GST_DEBUG_OBJECT (pool, "allocated gst buffer: %p, memory: %p", buf, mem);

  *buffer = buf;
  return GST_FLOW_OK;

no_buf:
  {
    GST_WARNING_OBJECT (pool, "alloc out buffer failed!");
    *buffer = NULL;
    return GST_FLOW_ERROR;
  }
}

static void
gst_qticodec2_buffer_pool_finalize (GObject * obj)
{
  GstQticodec2BufferPool *pool = GST_QTICODEC2_BUFFER_POOL_CAST (obj);

  GST_DEBUG_OBJECT (pool, "finalize buffer pool");

  if (pool->c2_allocator) {
    gst_object_unref (pool->c2_allocator);
    pool->c2_allocator = NULL;
  }

  G_OBJECT_CLASS (gst_qticodec2_buffer_pool_parent_class)->finalize (obj);
}

static void
gst_qticodec2_buffer_pool_class_init (GstQticodec2BufferPoolClass * klass)
{
  GObjectClass *gobj_class = (GObjectClass *) klass;
  GstBufferPoolClass *bp_class = (GstBufferPoolClass *) klass;

  gobj_class->finalize = gst_qticodec2_buffer_pool_finalize;

  bp_class->get_options = gst_qticodec2_buffer_pool_get_options;
  bp_class->set_config = gst_qticodec2_buffer_pool_set_config;
  bp_class->alloc_buffer = gst_qticodec2_buffer_pool_alloc;
}

GstBufferPool *
gst_qticodec2_buffer_pool_new (gpointer comp, BUFFER_POOL_TYPE pool_type,
    guint num_buffers, GstCaps * caps)
{
  GstQticodec2BufferPool *pool;
  GstStructure *config;
  GstVideoInfo info;
  GstAllocationParams params = { 0, 0, 0, 0, };

  pool = (GstQticodec2BufferPool *)
      g_object_new (GST_TYPE_QTICODEC2_BUFFER_POOL, NULL);

  GST_INFO_OBJECT (pool, "gst_qticodec2_buffer_pool_new, type: %d, num_buffers: %d",
      pool_type, num_buffers);

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (pool, "failed to get video info");
    return NULL;
  }

  /* create allocator for pool */
  pool->c2_allocator = gst_qticodec2_allocator_new (comp, pool_type, caps);

  config = gst_buffer_pool_get_config (GST_BUFFER_POOL_CAST (pool));

  /* set pool params and options */
  gst_buffer_pool_config_set_params (config, caps, info.size, 0, num_buffers);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);

  /* set allocator */
  gst_buffer_pool_config_set_allocator (config,
      pool->c2_allocator, &params);

  if (!gst_qticodec2_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (pool, "failed to set config to pool");
    return NULL;
  }

  GST_INFO_OBJECT (pool, "created buffer pool %p", pool);

  return GST_BUFFER_POOL (pool);
}
