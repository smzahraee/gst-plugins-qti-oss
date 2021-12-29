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


#ifndef __GST_QTICODEC2VDECBUFFERPOOL_H__
#define __GST_QTICODEC2VDECBUFFERPOOL_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideodecoder.h>
#include <gst/video/gstvideopool.h>
#include <gst/allocators/allocators.h>
#include <gst/allocators/gstdmabuf.h>
#include "gstqticodec2vdec.h"

G_BEGIN_DECLS

typedef struct _Gstqticodec2vdecBufferPool Gstqticodec2vdecBufferPool;
typedef struct _Gstqticodec2vdecBufferPoolClass Gstqticodec2vdecBufferPoolClass;

/* buffer pool functions */
#define GST_TYPE_QTICODEC2VDEC_BUFFER_POOL      (gst_qticodec2vdec_buffer_pool_get_type())
#define GST_IS_QTICODEC2VDEC_BUFFER_POOL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_QTICODEC2VDEC_BUFFER_POOL))
#define GST_QTICODEC2VDEC_BUFFER_POOL(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_QTICODEC2VDEC_BUFFER_POOL, Gstqticodec2vdecBufferPool))
#define GST_QTICODEC2VDEC_BUFFER_POOL_CAST(obj) ((Gstqticodec2vdecBufferPool*)(obj))

struct _Gstqticodec2vdecBufferPool
{
  GstBufferPool bufferpool;
  Gstqticodec2vdec *qticodec2vdec;
  GstAllocator *allocator;
  GHashTable *buffer_table;
};

struct _Gstqticodec2vdecBufferPoolClass
{
  GstBufferPoolClass parent_class;
};

typedef struct GstBufferPoolAcquireParamsExt {
  GstBufferPoolAcquireParams params;
  gint32 fd;
  gint32 meta_fd;
  guint64 index;
  guint32 size;
} GstBufferPoolAcquireParamsExt;

GType gst_qticodec2vdec_buffer_pool_get_type (void);
GstBufferPool *gst_qticodec2vdec_buffer_pool_new (Gstqticodec2vdec * qticodec2vdec, GstAllocator * allocator,
                                                  GHashTable *buffer_table);

G_END_DECLS

#endif /* __GST_QTICODEC2VDECBUFFERPOOL_H__ */
