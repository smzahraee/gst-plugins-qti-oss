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

#ifndef __GST_QTI_ML_VIDEO_SEGMENTATION_MODULE_H__
#define __GST_QTI_ML_VIDEO_SEGMENTATION_MODULE_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/**
 * gst_ml_video_segmentation_module_init:
 * @labels: filename or a GST string containing labels information
 *
 * Initilize instance of the image segmentation module.
 *
 * return: pointer to a privatre module struct on success or NULL on failure
 */
GST_API gpointer
gst_ml_video_segmentation_module_init (const gchar * labels);

/**
 * gst_ml_video_segmentation_module_deinit:
 * @instance: pointer to the private module structure
 *
 * Deinitialize the instance of the image segmentation module.
 *
 * return: NONE
 */
GST_API void
gst_ml_video_segmentation_module_deinit (gpointer instance);

/**
 * gst_ml_video_segmentation_module_process:
 * @instance: pointer to the private module structure
 * @inbuffer: buffer containing tensor memory blocks that need processing
 * @outbuffer: buffer where the processed tensor data will be placed
 *
 * Parses incoming buffer containing result tensors from a image segmentation
 * model and converts that information into an output image mask.
 *
 * return: TRUE on success or FALSE on failure
 */
GST_API gboolean
gst_ml_video_segmentation_module_process (gpointer instance, GstBuffer * inbuffer,
                                          GstBuffer * outbuffer);

G_END_DECLS

#endif // __GST_QTI_ML_VIDEO_SEGMENTATION_MODULE_H__
