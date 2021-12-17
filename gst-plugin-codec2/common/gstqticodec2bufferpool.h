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


#ifndef __GST_QTICODEC2_BUFFER_POOL__
#define __GST_QTICODEC2_BUFFER_POOL__

#include "codec2wrapper.h"
#include <gst/allocators/allocators.h>

G_BEGIN_DECLS

typedef struct _GstQticodec2Allocator GstQticodec2Allocator;
typedef struct _GstQticodec2AllocatorClass GstQticodec2AllocatorClass;

#define GST_TYPE_QTICODEC2_ALLOCATOR      (gst_qticodec2_allocator_get_type())
#define GST_QTICODEC2_ALLOCATOR(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_QTICODEC2_ALLOCATOR, GstQticodec2Allocator))
#define GST_QTICODEC2_ALLOCATOR_CAST(obj) ((GstQticodec2Allocator*)(obj))

GType
gst_qticodec2_allocator_get_type (void);

struct _GstQticodec2Allocator
{
  GstFdAllocator   parent;    // parent
  GstVideoInfo*    info;      // info used for allocation
  void*            comp;      // c2 component
  BUFFER_POOL_TYPE pool_type; // graphic or linear
  gsize            alloc_size; // allocation size
};

struct _GstQticodec2AllocatorClass
{
  GstFdAllocatorClass parent_class;

  gpointer _gst_reserved[GST_PADDING];
};

GstAllocator *
gst_qticodec2_allocator_new (gpointer comp, BUFFER_POOL_TYPE pool_type,
    GstCaps * caps);

typedef struct _GstQticodec2BufferPool GstQticodec2BufferPool;
typedef struct _GstQticodec2BufferPoolClass GstQticodec2BufferPoolClass;

/* buffer pool functions */
#define GST_TYPE_QTICODEC2_BUFFER_POOL      (gst_qticodec2_buffer_pool_get_type())
#define GST_IS_QTICODEC2_BUFFER_POOL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_QTICODEC2_BUFFER_POOL))
#define GST_QTICODEC2_BUFFER_POOL(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_QTICODEC2_BUFFER_POOL, GstQticodec2BufferPool))
#define GST_QTICODEC2_BUFFER_POOL_CAST(obj) ((GstQticodec2BufferPool*)(obj))

struct _GstQticodec2BufferPool
{
  GstBufferPool bufferpool;
  GstAllocator *c2_allocator;
};

struct _GstQticodec2BufferPoolClass
{
  GstBufferPoolClass parent_class;
};

GType
gst_qticodec2_buffer_pool_get_type (void);

GstBufferPool *
gst_qticodec2_buffer_pool_new (gpointer comp, BUFFER_POOL_TYPE pool_type,
    guint num_buffers, GstCaps * caps);

G_END_DECLS

#endif /* __GST_QTICODEC2_BUFFER_POOL__ */
