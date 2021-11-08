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

#include "mlsnpe.h"

#include <gst/ml/gstmlpool.h>
#include <gst/ml/gstmlmeta.h>

#define GST_CAT_DEFAULT gst_ml_snpe_debug
GST_DEBUG_CATEGORY_STATIC (gst_ml_snpe_debug);

#define gst_ml_snpe_parent_class parent_class
G_DEFINE_TYPE (GstMLSnpe, gst_ml_snpe, GST_TYPE_BASE_TRANSFORM);

#define DEFAULT_PROP_MODEL       NULL
#define DEFAULT_PROP_DELEGATE    GST_ML_SNPE_DELEGATE_NONE

#define DEFAULT_PROP_MIN_BUFFERS 2
#define DEFAULT_PROP_MAX_BUFFERS 10

#define GST_ML_SNPE_TENSOR_TYPES "{ UINT8, INT32, FLOAT32 }"

#define GST_ML_SNPE_CAPS                        \
    "neural-network/tensors, "                    \
    "type = (string) " GST_ML_SNPE_TENSOR_TYPES

enum
{
  PROP_0,
  PROP_MODEL,
  PROP_DELEGATE,
  PROP_LAYERS,
};

static GstStaticCaps gst_ml_snpe_static_caps =
    GST_STATIC_CAPS (GST_ML_SNPE_CAPS);

static GstCaps *
gst_ml_snpe_src_caps (void)
{
  static GstCaps *caps = NULL;
  static volatile gsize inited = 0;

  if (g_once_init_enter (&inited)) {
    caps = gst_static_caps_get (&gst_ml_snpe_static_caps);
    g_once_init_leave (&inited, 1);
  }
  return caps;
}

static GstCaps *
gst_ml_snpe_sink_caps (void)
{
  static GstCaps *caps = NULL;
  static volatile gsize inited = 0;

  if (g_once_init_enter (&inited)) {
    caps = gst_static_caps_get (&gst_ml_snpe_static_caps);
    g_once_init_leave (&inited, 1);
  }
  return caps;
}

static GstPadTemplate *
gst_ml_snpe_src_template (void)
{
  return gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      gst_ml_snpe_src_caps ());
}

static GstPadTemplate *
gst_ml_snpe_sink_template (void)
{
  return gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      gst_ml_snpe_sink_caps ());
}

static GstBufferPool *
gst_ml_snpe_create_pool (GstMLSnpe * snpe, GstCaps * caps)
{
  GstBufferPool *pool = NULL;
  GstStructure *config = NULL;
  GstAllocator *allocator = NULL;
  GstMLInfo info;

  if (!gst_ml_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (snpe, "Invalid caps %" GST_PTR_FORMAT, caps);
    return NULL;
  }

  // GST_INFO_OBJECT (snpe, "Uses ION memory");
  // pool = gst_ml_buffer_pool_new (GST_ML_BUFFER_POOL_TYPE_ION);
  GST_INFO_OBJECT (snpe, "Uses SYSTEM memory");
  pool = gst_ml_buffer_pool_new (GST_ML_BUFFER_POOL_TYPE_SYSTEM);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, gst_ml_info_size (&info),
      DEFAULT_PROP_MIN_BUFFERS, DEFAULT_PROP_MAX_BUFFERS);

  // allocator = gst_fd_allocator_new ();
  allocator = gst_allocator_find (GST_ALLOCATOR_SYSMEM);

  gst_buffer_pool_config_set_allocator (config, allocator, NULL);
  gst_buffer_pool_config_add_option (
      config, GST_ML_BUFFER_POOL_OPTION_TENSOR_META);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_WARNING_OBJECT (snpe, "Failed to set pool configuration!");
    g_object_unref (pool);
    pool = NULL;
  }
  g_object_unref (allocator);

  return pool;
}

static gboolean
gst_ml_snpe_propose_allocation (GstBaseTransform * base,
    GstQuery * inquery, GstQuery * outquery)
{
  GstMLSnpe *snpe = GST_ML_SNPE (base);

  GstCaps *caps = NULL;
  GstBufferPool *pool = NULL;
  GstMLInfo info;
  guint size = 0;
  gboolean needpool = FALSE;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (
        base, inquery, outquery))
    return FALSE;

  // No input query, nothing to do.
  if (NULL == inquery)
    return TRUE;

  // Extract caps from the query.
  gst_query_parse_allocation (outquery, &caps, &needpool);

  if (NULL == caps) {
    GST_ERROR_OBJECT (snpe, "Failed to extract caps from query!");
    return FALSE;
  }

  if (!gst_ml_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (snpe, "Failed to get ML info!");
    return FALSE;
  }

  // Get the size from ML info.
  size = gst_ml_info_size (&info);

  if (needpool) {
    GstStructure *structure = NULL;

    if ((pool = gst_ml_snpe_create_pool (snpe, caps)) == NULL) {
      GST_ERROR_OBJECT (snpe, "Failed to create buffer pool!");
      return FALSE;
    }

    structure = gst_buffer_pool_get_config (pool);

    // Set caps and size in query.
    gst_buffer_pool_config_set_params (structure, caps, size, 0, 0);

    if (!gst_buffer_pool_set_config (pool, structure)) {
      GST_ERROR_OBJECT (snpe, "Failed to set buffer pool configuration!");
      gst_object_unref (pool);
      return FALSE;
    }
  }

  // If upstream does't have a pool requirement, set only size in query.
  gst_query_add_allocation_pool (outquery, needpool ? pool : NULL, size, 0, 0);

  if (pool != NULL)
    gst_object_unref (pool);

  gst_query_add_allocation_meta (outquery, GST_ML_TENSOR_META_API_TYPE, NULL);
  return TRUE;
}

static gboolean
gst_ml_snpe_decide_allocation (GstBaseTransform * base, GstQuery * query)
{
  GstMLSnpe *snpe = GST_ML_SNPE (base);

  GstCaps *caps = NULL;
  GstBufferPool *pool = NULL;
  GstStructure *config = NULL;
  GstAllocator *allocator = NULL;
  guint size, minbuffers, maxbuffers;
  GstAllocationParams params;

  gst_query_parse_allocation (query, &caps, NULL);
  if (!caps) {
    GST_ERROR_OBJECT (snpe, "Failed to parse the decide_allocation caps!");
    return FALSE;
  }

  // Invalidate the cached pool if there is an allocation_query.
  if (snpe->outpool)
    gst_object_unref (snpe->outpool);

  // Create a new buffer pool.
  if ((pool = gst_ml_snpe_create_pool (snpe, caps)) == NULL) {
    GST_ERROR_OBJECT (snpe, "Failed to create buffer pool!");
    return FALSE;
  }

  snpe->outpool = pool;

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

  gst_query_add_allocation_meta (query, GST_ML_TENSOR_META_API_TYPE, NULL);
  return TRUE;
}

static GstFlowReturn
gst_ml_snpe_prepare_output_buffer (GstBaseTransform * base,
    GstBuffer * inbuffer, GstBuffer ** outbuffer)
{
  GstMLSnpe *snpe = GST_ML_SNPE (base);
  GstBufferPool *pool = snpe->outpool;
  GstFlowReturn ret = GST_FLOW_OK;

  if (gst_base_transform_is_passthrough (base)) {
    GST_DEBUG_OBJECT (snpe, "Passthrough, no need to do anything");
    *outbuffer = inbuffer;
    return GST_FLOW_OK;
  }

  if (!snpe->engine) {
    GST_WARNING_OBJECT (snpe, "Engine not created!");
    return GST_FLOW_NOT_NEGOTIATED;
  }

  g_return_val_if_fail (pool != NULL, GST_FLOW_ERROR);

  if (!gst_buffer_pool_is_active (pool) &&
      !gst_buffer_pool_set_active (pool, TRUE)) {
    GST_ERROR_OBJECT (snpe, "Failed to activate output buffer pool!");
    return GST_FLOW_ERROR;
  }

  ret = gst_buffer_pool_acquire_buffer (pool, outbuffer, NULL);
  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (snpe, "Failed to create output buffer!");
    return GST_FLOW_ERROR;
  }

  // Copy the flags and timestamps from the input buffer.
  gst_buffer_copy_into (*outbuffer, inbuffer,
      GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS, 0, -1);

  return GST_FLOW_OK;
}

static GstCaps *
gst_ml_snpe_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstMLSnpe *snpe = GST_ML_SNPE (base);
  GstCaps *result = NULL;
  const GstMLInfo *mlinfo = NULL;
  const GValue *value = NULL;

  if ((NULL == snpe->engine) && (filter != NULL))
    return gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
  else if (NULL == snpe->engine)
    return gst_caps_ref (caps);

  GST_DEBUG_OBJECT (snpe, "Transforming caps: %" GST_PTR_FORMAT
      " in direction %s", caps, (direction == GST_PAD_SINK) ? "sink" : "src");
  GST_DEBUG_OBJECT (snpe, "Filter caps: %" GST_PTR_FORMAT, filter);

  switch (direction) {
    case GST_PAD_SRC:
      mlinfo = gst_ml_snpe_engine_get_input_info (snpe->engine);
      break;
    case GST_PAD_SINK:
      mlinfo = gst_ml_snpe_engine_get_output_info (snpe->engine);
      break;
    default:
      GST_ERROR_OBJECT (snpe, "Invalid pad direction!");
      return NULL;
  }

  // The source and sink pads caps do not depend on each other so directly take
  // the ML caps from the engine for the corresponding pad and apply filter.
  result = gst_ml_info_to_caps (mlinfo);

  // Extract the aspect ratio.
  value = gst_structure_get_value (gst_caps_get_structure (caps, 0),
      "aspect-ratio");

  // Propagate aspect ratio to the ML caps if it exists.
  if (value != NULL)
    gst_caps_set_value (result, "aspect-ratio", value);

  // Extract the rate.
  value = gst_structure_get_value (gst_caps_get_structure (caps, 0), "rate");

  // Propagate rate to the ML caps if it exists.
  if (value != NULL)
    gst_caps_set_value (result, "rate", value);

  GST_DEBUG_OBJECT (snpe, "ML caps: %" GST_PTR_FORMAT, result);

  if (filter) {
    GstCaps *intersection  =
        gst_caps_intersect_full (filter, result, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (result);
    result = intersection;
  }

  GST_DEBUG_OBJECT (snpe, "Returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
gst_ml_snpe_accept_caps (GstBaseTransform * base, GstPadDirection direction,
    GstCaps * caps)
{
  GstMLSnpe *snpe = GST_ML_SNPE (base);
  GstCaps *mlcaps = NULL;
  const GstMLInfo *mlinfo = NULL;

  GST_DEBUG_OBJECT (snpe, "Accept caps: %" GST_PTR_FORMAT
      " in direction %s", caps, (direction == GST_PAD_SINK) ? "sink" : "src");

  if ((NULL == snpe->engine) && (direction == GST_PAD_SINK)) {
    mlcaps = gst_pad_get_pad_template_caps (GST_BASE_TRANSFORM_SINK_PAD (base));
  } else if ((NULL == snpe->engine) && (direction == GST_PAD_SRC)) {
    mlcaps = gst_pad_get_pad_template_caps (GST_BASE_TRANSFORM_SRC_PAD (base));
  } else if (direction == GST_PAD_SINK) {
    mlinfo = gst_ml_snpe_engine_get_input_info (snpe->engine);
  } else if (direction == GST_PAD_SRC) {
    mlinfo = gst_ml_snpe_engine_get_output_info (snpe->engine);
  }

  if ((mlinfo != NULL) && (NULL == mlcaps))
    mlcaps = gst_ml_info_to_caps (mlinfo);

  if (NULL == mlcaps) {
    GST_ERROR_OBJECT (base, "Failed to get ML caps!");
    return FALSE;
  }

  GST_DEBUG_OBJECT (snpe, "ML caps: %" GST_PTR_FORMAT, mlcaps);

  if (!gst_caps_can_intersect (caps, mlcaps)) {
    GST_WARNING_OBJECT (base, "Caps can't intersect!");
    return FALSE;
  }

  return TRUE;
}

static GstStateChangeReturn
gst_ml_snpe_change_state (GstElement * element, GstStateChange transition)
{
  GstMLSnpe *snpe = GST_ML_SNPE (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
    {
      GstStructure *settings = gst_structure_new_empty ("ml-engine-settings");
      GList *list = NULL;
      GValue layers = G_VALUE_INIT;

      gst_structure_set (settings,
          GST_ML_SNPE_ENGINE_OPT_MODEL, G_TYPE_STRING,
          snpe->model,
          GST_ML_SNPE_ENGINE_OPT_DELEGATE, GST_TYPE_ML_SNPE_DELEGATE,
          snpe->delegate,
          NULL);

      g_value_init (&layers, GST_TYPE_ARRAY);

      for (list = snpe->layers; list != NULL; list = list->next) {
        const gchar *layer = list->data;
        GValue value = G_VALUE_INIT;

        g_value_init (&value, G_TYPE_STRING);
        g_value_set_string (&value, layer);

        gst_value_array_append_value (&layers, &value);
        g_value_unset (&value);
      }

      gst_structure_set_value (settings, GST_ML_SNPE_ENGINE_OPT_LAYERS,
          &layers);

      gst_ml_snpe_engine_free (snpe->engine);

      snpe->engine = gst_ml_snpe_engine_new (settings);
      if (NULL == snpe->engine) {
        GST_ERROR_OBJECT (snpe, "Failed to create engine!");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret != GST_STATE_CHANGE_SUCCESS) {
    GST_ERROR_OBJECT (snpe, "Failure");
    return ret;
  }

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_ml_snpe_engine_free (snpe->engine);
      snpe->engine = NULL;
      break;
    default:
      // This is to catch PAUSED->PAUSED and PLAYING->PLAYING transitions.
      ret = (GST_STATE_TRANSITION_NEXT (transition) == GST_STATE_PAUSED) ?
          GST_STATE_CHANGE_NO_PREROLL : GST_STATE_CHANGE_SUCCESS;
      break;
  }

  return ret;
}

static GstFlowReturn
gst_ml_snpe_transform (GstBaseTransform * base, GstBuffer * inbuffer,
    GstBuffer * outbuffer)
{
  GstMLSnpe *snpe = GST_ML_SNPE (base);
  GstClockTime ts_begin = GST_CLOCK_TIME_NONE, ts_end = GST_CLOCK_TIME_NONE;
  GstClockTimeDiff tsdelta = GST_CLOCK_STIME_NONE;

  ts_begin = gst_util_get_timestamp ();

  gst_ml_snpe_engine_execute (snpe->engine, inbuffer, outbuffer);

  ts_end = gst_util_get_timestamp ();

  tsdelta = GST_CLOCK_DIFF (ts_begin, ts_end);

  GST_LOG_OBJECT (snpe, "Execute took %" G_GINT64_FORMAT ".%03"
      G_GINT64_FORMAT " ms", GST_TIME_AS_MSECONDS (tsdelta),
      (GST_TIME_AS_USECONDS (tsdelta) % 1000));

  return GST_FLOW_OK;
}

static void
gst_ml_snpe_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMLSnpe *snpe = GST_ML_SNPE (object);

  switch (prop_id) {
    case PROP_MODEL:
      g_free (snpe->model);
      snpe->model = g_strdup (g_value_get_string (value));
      break;
    case PROP_DELEGATE:
      snpe->delegate = g_value_get_enum (value);
      break;
    case PROP_LAYERS:
    {
      guint idx = 0;

      g_list_free_full (snpe->layers, (GDestroyNotify) g_free);
      snpe->layers = NULL;

      for (idx = 0; idx < gst_value_array_get_size (value); idx++) {
        const gchar *layer = g_value_get_string (
            gst_value_array_get_value (value, idx));
        snpe->layers = g_list_append (snpe->layers, g_strdup (layer));
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ml_snpe_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstMLSnpe *snpe = GST_ML_SNPE (object);

  switch (prop_id) {
    case PROP_MODEL:
      g_value_set_string (value, snpe->model);
      break;
    case PROP_DELEGATE:
      g_value_set_enum (value, snpe->delegate);
      break;
    case PROP_LAYERS:
    {
      GList *list = NULL;
      GValue val = G_VALUE_INIT;

      for (list = snpe->layers; list != NULL; list = list->next) {
        const gchar *layer = list->data;

        g_value_init (&val, G_TYPE_STRING);
        g_value_set_string (&val, layer);

        gst_value_array_append_value (value, &val);
        g_value_unset (&val);
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ml_snpe_finalize (GObject * object)
{
  GstMLSnpe *snpe = GST_ML_SNPE (object);

  gst_ml_snpe_engine_free (snpe->engine);

  if (snpe->outpool != NULL)
    gst_object_unref (snpe->outpool);

  g_free (snpe->model);

  g_list_free_full (snpe->layers, (GDestroyNotify) g_free);

  G_OBJECT_CLASS (parent_class)->finalize (G_OBJECT (snpe));
}

static void
gst_ml_snpe_class_init (GstMLSnpeClass * klass)
{
  GObjectClass *gobject       = G_OBJECT_CLASS (klass);
  GstElementClass *element    = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *base = GST_BASE_TRANSFORM_CLASS (klass);

  gobject->set_property = GST_DEBUG_FUNCPTR (gst_ml_snpe_set_property);
  gobject->get_property = GST_DEBUG_FUNCPTR (gst_ml_snpe_get_property);
  gobject->finalize     = GST_DEBUG_FUNCPTR (gst_ml_snpe_finalize);

  g_object_class_install_property (gobject, PROP_MODEL,
      g_param_spec_string ("model", "Model",
          "Model filename", DEFAULT_PROP_MODEL,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject, PROP_DELEGATE,
      g_param_spec_enum ("delegate", "Delegate",
          "Delegate the graph execution to another executor",
          GST_TYPE_ML_SNPE_DELEGATE, DEFAULT_PROP_DELEGATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject, PROP_LAYERS,
     gst_param_spec_array ("layers", "Layers",
          "List of output layers. Should be set if model has more than one output",
          g_param_spec_string ("name", "Layer Name",
              "Name of the output layer.", NULL,
              G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element,
      "SNPE Machine Learning", "Filter/Effect/Converter",
      "SNPE based Machine Learning plugin", "QTI");

  gst_element_class_add_pad_template (element,
      gst_ml_snpe_sink_template ());
  gst_element_class_add_pad_template (element,
      gst_ml_snpe_src_template ());

  element->change_state = GST_DEBUG_FUNCPTR (gst_ml_snpe_change_state);

  base->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_ml_snpe_propose_allocation);
  base->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_ml_snpe_decide_allocation);
  base->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (gst_ml_snpe_prepare_output_buffer);

  base->transform_caps = GST_DEBUG_FUNCPTR (gst_ml_snpe_transform_caps);
  base->accept_caps = GST_DEBUG_FUNCPTR (gst_ml_snpe_accept_caps);

  base->transform = GST_DEBUG_FUNCPTR (gst_ml_snpe_transform);
}

static void
gst_ml_snpe_init (GstMLSnpe * snpe)
{
  snpe->outpool = NULL;
  snpe->engine = NULL;

  snpe->model = DEFAULT_PROP_MODEL;
  snpe->delegate = DEFAULT_PROP_DELEGATE;
  snpe->layers = NULL;

  GST_DEBUG_CATEGORY_INIT (gst_ml_snpe_debug, "qtimlsnpe", 0,
      "QTI SNPE ML plugin");
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "qtimlsnpe", GST_RANK_NONE,
      GST_TYPE_ML_SNPE);
}

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    qtimlsnpe,
    "QTI SNPE based Machine Learnig plugin",
    plugin_init,
    PACKAGE_VERSION,
    PACKAGE_LICENSE,
    PACKAGE_SUMMARY,
    PACKAGE_ORIGIN
)
