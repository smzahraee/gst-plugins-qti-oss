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

#include "mlvsegmentation.h"

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>

#include <gst/ml/gstmlpool.h>
#include <gst/ml/gstmlmeta.h>
#include <gst/video/gstimagepool.h>

#include "modules/ml-video-segmentation-module.h"

#define GST_CAT_DEFAULT gst_ml_video_segmentation_debug
GST_DEBUG_CATEGORY_STATIC (gst_ml_video_segmentation_debug);

#define gst_ml_video_segmentation_parent_class parent_class
G_DEFINE_TYPE (GstMLVideoSegmentation, gst_ml_video_segmentation,
               GST_TYPE_BASE_TRANSFORM);

#define DEFAULT_PROP_MODULE         NULL
#define DEFAULT_PROP_LABELS         NULL

#define DEFAULT_MIN_BUFFERS         2
#define DEFAULT_MAX_BUFFERS         10

#ifndef GST_CAPS_FEATURE_MEMORY_GBM
#define GST_CAPS_FEATURE_MEMORY_GBM "memory:GBM"
#endif

#define GST_ML_VIDEO_SEGMENTATION_VIDEO_FORMATS \
    "{ BGRA, RGBA, BGRx, xRGB }"

#define GST_ML_VIDEO_SEGMENTATION_SRC_CAPS                            \
    "video/x-raw, "                                                   \
    "format = (string) " GST_ML_VIDEO_SEGMENTATION_VIDEO_FORMATS "; " \
    "video/x-raw(" GST_CAPS_FEATURE_MEMORY_GBM "), "                  \
    "format = (string) " GST_ML_VIDEO_SEGMENTATION_VIDEO_FORMATS

#define GST_ML_VIDEO_SEGMENTATION_SINK_CAPS \
    "neural-network/tensors"

enum
{
  PROP_0,
  PROP_MODULE,
  PROP_LABELS,
};

static GstStaticCaps gst_ml_video_segmentation_static_sink_caps =
    GST_STATIC_CAPS (GST_ML_VIDEO_SEGMENTATION_SINK_CAPS);

static GstStaticCaps gst_ml_video_segmentation_static_src_caps =
    GST_STATIC_CAPS (GST_ML_VIDEO_SEGMENTATION_SRC_CAPS);


/**
 * GstMLModule:
 * @libhandle: the library handle
 * @instance: instance of the tensor processing module
 *
 * @init: Initilizes an instance of the module.
 * @deinit: Deinitilizes the instance of the module.
 * @process: Decode the tensors inside the buffer into prediction results.
 *
 * Machine learning interface for post-processing module.
 */
struct _GstMLModule
{
  gpointer libhandle;
  gpointer instance;

  /// Virtual functions.
  gpointer (*init)    (const gchar * labels);
  void     (*deinit)  (gpointer instance);

  gboolean (*process) (gpointer instance, GstBuffer * inbuffer,
                       GstBuffer * outbuffer);
};

static void
gst_ml_module_free (GstMLModule * module)
{
  if (NULL == module)
    return;

  if (module->instance != NULL)
    module->deinit (module->instance);

  if (module->libhandle != NULL)
    dlclose (module->libhandle);

  g_slice_free (GstMLModule, module);
}

static GstMLModule *
gst_ml_module_new (const gchar * libname, const gchar * labels)
{
  GstMLModule *module = NULL;
  gchar *location = NULL;

  location = g_strdup_printf ("%s/lib%s.so", GST_ML_MODULES_DIR, libname);

  module = g_slice_new0 (GstMLModule);
  g_return_val_if_fail (module != NULL, NULL);

  if ((module->libhandle = dlopen (location, RTLD_NOW)) == NULL) {
    GST_ERROR ("Failed to open %s module library, error: %s!",
        libname, dlerror());

    g_free (location);
    gst_ml_module_free (module);

    return NULL;
  }

  g_free (location);

  module->init = dlsym (module->libhandle,
      "gst_ml_video_segmentation_module_init");
  module->deinit = dlsym (module->libhandle,
      "gst_ml_video_segmentation_module_deinit");
  module->process = dlsym (module->libhandle,
      "gst_ml_video_segmentation_module_process");

  if (!module->init || !module->deinit || !module->process) {
    GST_ERROR ("Failed to load %s library symbols, error: %s!",
        libname, dlerror());
    gst_ml_module_free (module);
    return NULL;
  }

  if ((module->instance = module->init (labels)) == NULL) {
    GST_ERROR ("Failed to initilize %s module library!", libname);
    gst_ml_module_free (module);
    return NULL;
  }

  return module;
}

static GstCaps *
gst_ml_video_segmentation_sink_caps (void)
{
  static GstCaps *caps = NULL;
  static volatile gsize inited = 0;

  if (g_once_init_enter (&inited)) {
    caps = gst_static_caps_get (&gst_ml_video_segmentation_static_sink_caps);
    g_once_init_leave (&inited, 1);
  }
  return caps;
}

static GstCaps *
gst_ml_video_segmentation_src_caps (void)
{
  static GstCaps *caps = NULL;
  static volatile gsize inited = 0;

  if (g_once_init_enter (&inited)) {
    caps = gst_static_caps_get (&gst_ml_video_segmentation_static_src_caps);
    g_once_init_leave (&inited, 1);
  }
  return caps;
}

static GstPadTemplate *
gst_ml_video_segmentation_sink_template (void)
{
  return gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      gst_ml_video_segmentation_sink_caps ());
}

static GstPadTemplate *
gst_ml_video_segmentation_src_template (void)
{
  return gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      gst_ml_video_segmentation_src_caps ());
}

static gboolean
caps_has_feature (const GstCaps * caps, const gchar * feature)
{
  guint idx = 0;

  while (idx != gst_caps_get_size (caps)) {
    GstCapsFeatures *const features = gst_caps_get_features (caps, idx);

    // Skip ANY caps and return immediately if feature is present.
    if (!gst_caps_features_is_any (features) &&
        gst_caps_features_contains (features, feature))
      return TRUE;

    idx++;
  }
  return FALSE;
}

static GstBufferPool *
gst_ml_video_segmentation_create_pool (GstMLVideoSegmentation * segmentation,
    GstCaps * caps)
{
  GstBufferPool *pool = NULL;
  GstStructure *config = NULL;
  GstAllocator *allocator = NULL;
  GstVideoInfo info;
  guint size = 0;


  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (segmentation, "Invalid caps %" GST_PTR_FORMAT, caps);
    return NULL;
  }

  // If downstream allocation query supports GBM, allocate gbm memory.
  if (caps_has_feature (caps, GST_CAPS_FEATURE_MEMORY_GBM)) {
    GST_INFO_OBJECT (segmentation, "Uses GBM memory");
    pool = gst_image_buffer_pool_new (GST_IMAGE_BUFFER_POOL_TYPE_GBM);
  } else {
    GST_INFO_OBJECT (segmentation, "Uses ION memory");
    pool = gst_image_buffer_pool_new (GST_IMAGE_BUFFER_POOL_TYPE_ION);
  }

  if (NULL == pool) {
    GST_ERROR_OBJECT (segmentation, "Failed to create buffer pool!");
    return NULL;
  }

  size = GST_VIDEO_INFO_SIZE (&info);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, size,
      DEFAULT_MIN_BUFFERS, DEFAULT_MAX_BUFFERS);

  allocator = gst_fd_allocator_new ();
  gst_buffer_pool_config_set_allocator (config, allocator, NULL);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_WARNING_OBJECT (segmentation, "Failed to set pool configuration!");
    g_object_unref (pool);
    pool = NULL;
  }
  g_object_unref (allocator);

  return pool;
}

static gboolean
gst_ml_video_segmentation_decide_allocation (GstBaseTransform * base,
    GstQuery * query)
{
  GstMLVideoSegmentation *segmentation = GST_ML_VIDEO_SEGMENTATION (base);

  GstCaps *caps = NULL;
  GstBufferPool *pool = NULL;
  GstStructure *config = NULL;
  GstAllocator *allocator = NULL;
  guint size, minbuffers, maxbuffers;
  GstAllocationParams params;

  gst_query_parse_allocation (query, &caps, NULL);
  if (!caps) {
    GST_ERROR_OBJECT (segmentation, "Failed to parse the allocation caps!");
    return FALSE;
  }

  // Invalidate the cached pool if there is an allocation_query.
  if (segmentation->outpool)
    gst_object_unref (segmentation->outpool);

  // Create a new buffer pool.
  pool = gst_ml_video_segmentation_create_pool (segmentation, caps);
  if (pool == NULL) {
    GST_ERROR_OBJECT (segmentation, "Failed to create buffer pool!");
    return FALSE;
  }

  segmentation->outpool = pool;

  // Get the configured pool properties in order to set in query.
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, &caps, &size, &minbuffers,
      &maxbuffers);

  if (gst_buffer_pool_config_get_allocator (config, &allocator, &params))
    gst_query_add_allocation_param (query, allocator, &params);

  gst_structure_free (config);

  // Check whether the query has pool.
  if (gst_query_get_n_allocation_pools (query) > 0)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, minbuffers,
        maxbuffers);
  else
    gst_query_add_allocation_pool (query, pool, size, minbuffers,
        maxbuffers);

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return TRUE;
}

static GstFlowReturn
gst_ml_video_segmentation_prepare_output_buffer (GstBaseTransform * base,
    GstBuffer * inbuffer, GstBuffer ** outbuffer)
{
  GstMLVideoSegmentation *segmentation = GST_ML_VIDEO_SEGMENTATION (base);
  GstBufferPool *pool = segmentation->outpool;
  GstFlowReturn ret = GST_FLOW_OK;

  if (gst_base_transform_is_passthrough (base)) {
    GST_TRACE_OBJECT (segmentation, "Passthrough, no need to do anything");
    *outbuffer = inbuffer;
    return GST_FLOW_OK;
  } else if (gst_base_transform_is_in_place (base)) {
    GST_TRACE_OBJECT (segmentation, "Inplace, use input buffer as output");
    *outbuffer = inbuffer;
    return GST_FLOW_OK;
  }

  g_return_val_if_fail (pool != NULL, GST_FLOW_ERROR);

  if (!gst_buffer_pool_is_active (pool) &&
      !gst_buffer_pool_set_active (pool, TRUE)) {
    GST_ERROR_OBJECT (segmentation, "Failed to activate output buffer pool!");
    return GST_FLOW_ERROR;
  }

  ret = gst_buffer_pool_acquire_buffer (pool, outbuffer, NULL);
  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (segmentation, "Failed to create output buffer!");
    return GST_FLOW_ERROR;
  }

  // Copy the flags and timestamps from the input buffer.
  gst_buffer_copy_into (*outbuffer, inbuffer,
      GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS, 0, -1);

  return GST_FLOW_OK;
}

static GstCaps *
gst_ml_video_segmentation_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstMLVideoSegmentation *segmentation = GST_ML_VIDEO_SEGMENTATION (base);
  GstCaps *result = NULL;
  const GValue *value = NULL;

  GST_DEBUG_OBJECT (segmentation, "Transforming caps: %" GST_PTR_FORMAT
      " in direction %s", caps, (direction == GST_PAD_SINK) ? "sink" : "src");
  GST_DEBUG_OBJECT (segmentation, "Filter caps: %" GST_PTR_FORMAT, filter);

  if (direction == GST_PAD_SRC) {
    GstPad *pad = GST_BASE_TRANSFORM_SINK_PAD (base);
    result = gst_pad_get_pad_template_caps (pad);
  } else if (direction == GST_PAD_SINK) {
    GstPad *pad = GST_BASE_TRANSFORM_SRC_PAD (base);
    result = gst_pad_get_pad_template_caps (pad);
  }

  // Extract the rate and propagate it to result caps.
  value = gst_structure_get_value (gst_caps_get_structure (caps, 0),
      (direction == GST_PAD_SRC) ? "framerate" : "rate");

  if (value != NULL) {
    gint idx = 0, length = 0;

    result = gst_caps_make_writable (result);
    length = gst_caps_get_size (result);

    for (idx = 0; idx < length; idx++) {
      GstStructure *structure = gst_caps_get_structure (result, idx);
      gst_structure_set_value (structure,
          (direction == GST_PAD_SRC) ? "rate" : "framerate", value);
    }
  }

  if (filter != NULL) {
    GstCaps *intersection  =
        gst_caps_intersect_full (filter, result, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (result);
    result = intersection;
  }

  GST_DEBUG_OBJECT (segmentation, "Returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

static GstCaps *
gst_ml_video_segmentation_fixate_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * incaps, GstCaps * outcaps)
{
  GstMLVideoSegmentation *segmentation = GST_ML_VIDEO_SEGMENTATION (base);
  GstStructure *output = NULL;
  GstMLInfo mlinfo;
  gint width = 0, height = 0, par_n, par_d, sar_n, sar_d, num, den;
  const GValue *value = NULL;

  // Truncate and make the output caps writable.
  outcaps = gst_caps_truncate (outcaps);
  outcaps = gst_caps_make_writable (outcaps);

  output = gst_caps_get_structure (outcaps, 0);

  GST_DEBUG_OBJECT (segmentation, "Trying to fixate output caps %"
      GST_PTR_FORMAT " based on caps %" GST_PTR_FORMAT, outcaps, incaps);

  // Fixate the output format.
  value = gst_structure_get_value (output, "format");

  if (!gst_value_is_fixed (value)) {
    gst_structure_fixate_field (output, "format");
    value = gst_structure_get_value (output, "format");
  }

  GST_DEBUG_OBJECT (segmentation, "Output format fixed to: %s",
      g_value_get_string (value));

  // Extract source aspect ratio from ML caps.
  value = gst_structure_get_value (
      gst_caps_get_structure (incaps, 0), "aspect-ratio");

  if ((value != NULL) && gst_value_is_fixed (value)) {
    sar_d = gst_value_get_fraction_denominator (value);
    sar_n = gst_value_get_fraction_numerator (value);
  } else {
    sar_n = sar_d = 1;
  }

  // Fixate output PAR if not already fixated..
  value = gst_structure_get_value (output, "pixel-aspect-ratio");

  if ((NULL == value) || !gst_value_is_fixed (value)) {
    gst_structure_set (output, "pixel-aspect-ratio",
        GST_TYPE_FRACTION, 1, 1, NULL);
    value = gst_structure_get_value (output, "pixel-aspect-ratio");
  }

  par_d = gst_value_get_fraction_denominator (value);
  par_n = gst_value_get_fraction_numerator (value);

  if (par_n != par_d) {
    GST_ERROR_OBJECT (segmentation, "Output PAR other than 1/1 not supported!");
    return NULL;
  }

  GST_DEBUG_OBJECT (segmentation, "Output PAR fixed to: %d/%d", par_n, par_d);

  // Calculate output dimensions scale factor from output PAR and source AR.
  gst_util_fraction_multiply (sar_n, sar_d, par_n, par_d, &num, &den);

  gst_ml_info_from_caps (&mlinfo, incaps);

  value = gst_structure_get_value (output, "width");

  if ((NULL == value) || !gst_value_is_fixed (value)) {
    // 2nd dimension correspond to height, 3rd dimension correspond to width.
    width = (num >= den) ? mlinfo.tensors[0][2] :
        gst_util_uint64_scale_int (mlinfo.tensors[0][1], num, den);
    width = GST_ROUND_DOWN_16 (width);

    gst_structure_set (output, "width", G_TYPE_INT, width, NULL);
    gst_structure_get_int (output, "width", &width);
  } else {
    gst_structure_get_int (output, "width", &width);

    if (((guint) width) > mlinfo.tensors[0][2]) {
      GST_ERROR_OBJECT (segmentation, "Fixated width is above the allowed "
          "max width of %u !", mlinfo.tensors[0][2]);
      return NULL;
    }
  }

  value = gst_structure_get_value (output, "height");

  if ((NULL == value) || !gst_value_is_fixed (value)) {
    // 2nd dimension correspond to height, 3rd dimension correspond to width.
    height = (num <= den) ? mlinfo.tensors[0][1] :
        gst_util_uint64_scale_int (
              GST_ROUND_DOWN_16 (mlinfo.tensors[0][2]), den, num);

    gst_structure_set (output, "height", G_TYPE_INT, height, NULL);
    gst_structure_get_int (output, "height", &height);
  } else {
    gst_structure_get_int (output, "height", &height);

    if (((guint) height) > mlinfo.tensors[0][1]) {
      GST_ERROR_OBJECT (segmentation, "Fixated height is above the allowed "
          "max height of %u !", mlinfo.tensors[0][1]);
      return NULL;
    }
  }

  GST_DEBUG_OBJECT (segmentation, "Output width and height fixated to: %dx%d",
      width, height);

  GST_DEBUG_OBJECT (segmentation, "Fixated caps to %" GST_PTR_FORMAT, outcaps);

  return outcaps;
}

static gboolean
gst_ml_video_segmentation_set_caps (GstBaseTransform * base, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstMLVideoSegmentation *segmentation = GST_ML_VIDEO_SEGMENTATION (base);
  GstMLModule *module = NULL;
  GstMLInfo ininfo;
  GstVideoInfo outinfo;

  if (NULL == segmentation->labels) {
    GST_ERROR_OBJECT (segmentation, "Labels not set!");
    return FALSE;
  } else if (NULL == segmentation->modname) {
    GST_ERROR_OBJECT (segmentation, "Module not set!");
    return FALSE;
  }

  module = gst_ml_module_new (segmentation->modname, segmentation->labels);
  if (NULL == module) {
    GST_ERROR_OBJECT (segmentation, "Failed to create processing module!");
    return FALSE;
  }

  gst_ml_module_free (segmentation->module);
  segmentation->module = module;

  if (!gst_ml_info_from_caps (&ininfo, incaps)) {
    GST_ERROR_OBJECT (segmentation, "Failed to get input ML info from caps %"
        GST_PTR_FORMAT "!", incaps);
    return FALSE;
  }

  if (!gst_video_info_from_caps (&outinfo, outcaps)) {
    GST_ERROR_OBJECT (segmentation, "Failed to get output video info from caps"
        " %" GST_PTR_FORMAT "!", outcaps);
    return FALSE;
  }

  gst_base_transform_set_passthrough (base, FALSE);
  gst_base_transform_set_in_place (base, FALSE);

  if (segmentation->mlinfo != NULL)
    gst_ml_info_free (segmentation->mlinfo);

  segmentation->mlinfo = gst_ml_info_copy (&ininfo);

  if (segmentation->vinfo != NULL)
    gst_video_info_free (segmentation->vinfo);

  segmentation->vinfo = gst_video_info_copy (&outinfo);

  GST_DEBUG_OBJECT (segmentation, "Input caps: %" GST_PTR_FORMAT, incaps);
  GST_DEBUG_OBJECT (segmentation, "Output caps: %" GST_PTR_FORMAT, outcaps);

  return TRUE;
}

static GstFlowReturn
gst_ml_video_segmentation_transform (GstBaseTransform * base,
    GstBuffer * inbuffer, GstBuffer * outbuffer)
{
  GstMLVideoSegmentation *segmentation = GST_ML_VIDEO_SEGMENTATION (base);
  GstClockTime ts_begin = GST_CLOCK_TIME_NONE, ts_end = GST_CLOCK_TIME_NONE;
  GstClockTimeDiff tsdelta = GST_CLOCK_STIME_NONE;
  gboolean success = FALSE;
  guint n_blocks = 0;

  g_return_val_if_fail (segmentation->module != NULL, GST_FLOW_ERROR);

  n_blocks = gst_buffer_n_memory (inbuffer);

  if (gst_buffer_get_size (inbuffer) != gst_ml_info_size (segmentation->mlinfo)) {
    GST_ERROR_OBJECT (segmentation, "Mismatch, expected buffer size %"
        G_GSIZE_FORMAT " but actual size is %" G_GSIZE_FORMAT "!",
        gst_ml_info_size (segmentation->mlinfo), gst_buffer_get_size (inbuffer));
    return FALSE;
  } else if ((n_blocks > 1) && n_blocks != segmentation->mlinfo->n_tensors) {
    GST_ERROR_OBJECT (segmentation, "Mismatch, expected %u memory blocks "
        "but buffer has %u!", segmentation->mlinfo->n_tensors, n_blocks);
    return FALSE;
  }

  n_blocks = gst_buffer_get_n_meta (inbuffer, GST_ML_TENSOR_META_API_TYPE);
  if (n_blocks != segmentation->mlinfo->n_tensors) {
    GST_ERROR_OBJECT (segmentation, "Input buffer has %u tensor metas but "
        "negotiated caps require %u!", n_blocks, segmentation->mlinfo->n_tensors);
    return GST_FLOW_ERROR;
  }

  if (gst_buffer_n_memory (outbuffer) == 0) {
    GST_ERROR_OBJECT (segmentation, "Output buffer has no memory blocks!");
    return GST_FLOW_ERROR;
  }

  ts_begin = gst_util_get_timestamp ();

  // Call the submodule process funtion.
  success = segmentation->module->process (
      segmentation->module->instance, inbuffer, outbuffer);

  if (!success) {
    GST_ERROR_OBJECT (segmentation, "Failed to process tensors!");
    return GST_FLOW_ERROR;
  }

  ts_end = gst_util_get_timestamp ();

  tsdelta = GST_CLOCK_DIFF (ts_begin, ts_end);

  GST_LOG_OBJECT (segmentation, "Segmentation took %" G_GINT64_FORMAT ".%03"
      G_GINT64_FORMAT " ms", GST_TIME_AS_MSECONDS (tsdelta),
      (GST_TIME_AS_USECONDS (tsdelta) % 1000));

  return GST_FLOW_OK;
}

static void
gst_ml_video_segmentation_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMLVideoSegmentation *segmentation = GST_ML_VIDEO_SEGMENTATION (object);

  switch (prop_id) {
    case PROP_MODULE:
      g_free (segmentation->modname);
      segmentation->modname = g_strdup (g_value_get_string (value));
      break;
    case PROP_LABELS:
      g_free (segmentation->labels);
      segmentation->labels = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ml_video_segmentation_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMLVideoSegmentation *segmentation = GST_ML_VIDEO_SEGMENTATION (object);

  switch (prop_id) {
    case PROP_MODULE:
      g_value_set_string (value, segmentation->modname);
      break;
    case PROP_LABELS:
      g_value_set_string (value, segmentation->labels);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ml_video_segmentation_finalize (GObject * object)
{
  GstMLVideoSegmentation *segmentation = GST_ML_VIDEO_SEGMENTATION (object);

  gst_ml_module_free (segmentation->module);

  if (segmentation->mlinfo != NULL)
    gst_ml_info_free (segmentation->mlinfo);

  if (segmentation->vinfo != NULL)
    gst_video_info_free (segmentation->vinfo);

  if (segmentation->outpool != NULL)
    gst_object_unref (segmentation->outpool);

  g_free (segmentation->modname);
  g_free (segmentation->labels);

  G_OBJECT_CLASS (parent_class)->finalize (G_OBJECT (segmentation));
}

static void
gst_ml_video_segmentation_class_init (GstMLVideoSegmentationClass * klass)
{
  GObjectClass *gobject       = G_OBJECT_CLASS (klass);
  GstElementClass *element    = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *base = GST_BASE_TRANSFORM_CLASS (klass);

  gobject->set_property =
      GST_DEBUG_FUNCPTR (gst_ml_video_segmentation_set_property);
  gobject->get_property =
      GST_DEBUG_FUNCPTR (gst_ml_video_segmentation_get_property);
  gobject->finalize = GST_DEBUG_FUNCPTR (gst_ml_video_segmentation_finalize);

  g_object_class_install_property (gobject, PROP_MODULE,
      g_param_spec_string ("module", "Module",
          "Module name that is going to be used for processing the tensors",
          DEFAULT_PROP_MODULE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject, PROP_LABELS,
      g_param_spec_string ("labels", "Labels",
          "Labels filename", DEFAULT_PROP_LABELS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element,
      "Machine Learning image segmentation", "Filter/Effect/Converter",
      "Machine Learning plugin for image segmentation", "QTI");

  gst_element_class_add_pad_template (element,
      gst_ml_video_segmentation_sink_template ());
  gst_element_class_add_pad_template (element,
      gst_ml_video_segmentation_src_template ());

  base->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_ml_video_segmentation_decide_allocation);
  base->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (gst_ml_video_segmentation_prepare_output_buffer);

  base->transform_caps =
      GST_DEBUG_FUNCPTR (gst_ml_video_segmentation_transform_caps);
  base->fixate_caps = GST_DEBUG_FUNCPTR (gst_ml_video_segmentation_fixate_caps);
  base->set_caps = GST_DEBUG_FUNCPTR (gst_ml_video_segmentation_set_caps);

  base->transform = GST_DEBUG_FUNCPTR (gst_ml_video_segmentation_transform);
}

static void
gst_ml_video_segmentation_init (GstMLVideoSegmentation * segmentation)
{
  segmentation->outpool = NULL;
  segmentation->module = NULL;

  segmentation->modname = DEFAULT_PROP_MODULE;
  segmentation->labels = DEFAULT_PROP_LABELS;

  GST_DEBUG_CATEGORY_INIT (gst_ml_video_segmentation_debug, "qtimlvsegmentation",
      0, "QTI ML image segmentation plugin");
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "qtimlvsegmentation", GST_RANK_NONE,
      GST_TYPE_ML_VIDEO_SEGMENTATION);
}

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    qtimlvsegmentation,
    "QTI Machine Learning plugin for image segmentation post processing",
    plugin_init,
    PACKAGE_VERSION,
    PACKAGE_LICENSE,
    PACKAGE_SUMMARY,
    PACKAGE_ORIGIN
)
