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
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __GST_VIDEO_COMPOSER_SINKPAD_H__
#define __GST_VIDEO_COMPOSER_SINKPAD_H__

#include <gst/gst.h>
#include <gst/base/gstaggregator.h>
#include <gst/video/video.h>

#include "videocomposerutils.h"

G_BEGIN_DECLS

#define GST_TYPE_VIDEO_COMPOSER_SINKPAD \
  (gst_video_composer_sinkpad_get_type())
#define GST_VIDEO_COMPOSER_SINKPAD(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VIDEO_COMPOSER_SINKPAD,\
                              GstVideoComposerSinkPad))
#define GST_VIDEO_COMPOSER_SINKPAD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VIDEO_COMPOSER_SINKPAD,\
                           GstVideoComposerSinkPadClass))
#define GST_IS_VIDEO_COMPOSER_SINKPAD(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VIDEO_COMPOSER_SINKPAD))
#define GST_IS_VIDEO_COMPOSER_SINKPAD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VIDEO_COMPOSER_SINKPAD))
#define GST_VIDEO_COMPOSER_SINKPAD_CAST(obj) ((GstVideoComposerSinkPad *)(obj))

#define GST_VIDEO_COMPOSER_SINKPAD_GET_LOCK(obj) \
  (&GST_VIDEO_COMPOSER_SINKPAD(obj)->lock)
#define GST_VIDEO_COMPOSER_SINKPAD_LOCK(obj) \
  g_mutex_lock(GST_VIDEO_COMPOSER_SINKPAD_GET_LOCK(obj))
#define GST_VIDEO_COMPOSER_SINKPAD_UNLOCK(obj) \
  g_mutex_unlock(GST_VIDEO_COMPOSER_SINKPAD_GET_LOCK(obj))

typedef struct _GstVideoComposerSinkPad GstVideoComposerSinkPad;
typedef struct _GstVideoComposerSinkPadClass GstVideoComposerSinkPadClass;

struct _GstVideoComposerSinkPad {
  /// Inherited parent structure.
  GstAggregatorPad        parent;

  /// Global mutex lock.
  GMutex                  lock;

  /// Sink pad index.
  guint                   index;
  /// Negotiated caps on the pad input parsed to video info.
  GstVideoInfo            *info;

  /// Properties.
  GstVideoRectangle       crop;
  GstVideoRectangle       destination;
  gboolean                flip_v;
  gboolean                flip_h;
  GstVideoComposerRotate  rotation;
  gdouble                 alpha;
  gint                    zorder;
};

struct _GstVideoComposerSinkPadClass {
  /// Inherited parent structure.
  GstAggregatorPadClass parent;
};

GType gst_video_composer_sinkpad_get_type (void);

gboolean
gst_video_composer_sinkpad_setcaps (GstAggregatorPad * sinkpad,
                                    GstAggregator * aggregator,
                                    GstCaps * caps);

gboolean
gst_video_composer_sinkpad_acceptcaps (GstAggregatorPad * sinkpad,
                                       GstAggregator * aggregator,
                                       GstCaps * caps);

GstCaps *
gst_video_composer_sinkpad_getcaps (GstAggregatorPad * sinkpad,
                                    GstAggregator * aggregator,
                                    GstCaps * filter);

G_END_DECLS

#endif // __GST_VIDEO_COMPOSER_SINKPAD_H__
