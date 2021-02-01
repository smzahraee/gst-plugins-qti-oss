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

#ifndef __CODEC2WRAPPER_H__
#define __CODEC2WRAPPER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <gmodule.h>

#define CONFIG_FUNCTION_KEY_PIXELFORMAT     "pixelformat"
#define CONFIG_FUNCTION_KEY_RESOLUTION      "resolution"
#define CONFIG_FUNCTION_KEY_BITRATE         "bitrate"
#define CONFIG_FUNCTION_KEY_INTERLACE       "interlace"

#define C2_TICKS_PER_SECOND 1000000

typedef enum  {
    BUFFER_POOL_BASIC_LINEAR = 0, 
    BUFFER_POOL_BASIC_GRAPHIC
} BUFFER_POOL_TYPE;

typedef enum {
    BLOCK_MODE_DONT_BLOCK = 0,
    BLOCK_MODE_MAY_BLOCK
} BLOCK_MODE_TYPE;

typedef enum {
    DRAIN_MODE_COMPONENT_WITH_EOS = 0,
    DRAIN_MODE_COMPONENT_NO_EOS,
    DRAIN_MODE_CHAIN
} DRAIN_MODE_TYPE;

typedef enum {
    FLUSH_MODE_COMPONENT = 0,
    FLUSH_MODE_CHAIN
} FLUSH_MODE_TYPE;

typedef enum {
    INTERLACE_MODE_PROGRESSIVE = 0,                  ///< progressive
    INTERLACE_MODE_INTERLEAVED_TOP_FIRST,            ///< line-interleaved. top-field-first
    INTERLACE_MODE_INTERLEAVED_BOTTOM_FIRST,         ///< line-interleaved. bottom-field-first
    INTERLACE_MODE_FIELD_TOP_FIRST,                  ///< field-sequential. top-field-first
    INTERLACE_MODE_FIELD_BOTTOM_FIRST,               ///< field-sequential. bottom-field-first
} INTERLACE_MODE_TYPE;

typedef enum {
    FLAG_TYPE_DROP_FRAME    = 1 << 0,
    FLAG_TYPE_END_OF_STREAM = 1 << 1,   ///< For input frames: no output frame shall be generated when processing this frame.
                                        ///< For output frames: this frame shall be discarded.
    FLAG_TYPE_DISCARD_FRAME = 1 << 2,   ///< This frame shall be discarded with its metadata.
    FLAG_TYPE_INCOMPLETE    = 1 << 3,   ///< This frame is not the last frame produced for the input
    FLAG_TYPE_CODEC_CONFIG  = 1 << 4    ///< Frame contains only codec-specific configuration data, and no actual access unit
} FLAG_TYPE;

typedef enum {
    PIXEL_FORMAT_NV12_LINEAR = 0,
    PIXEL_FORMAT_NV12_UBWC,
    PIXEL_FORMAT_RGBA_8888,
    PIXEL_FORMAT_YV12
} PIXEL_FORMAT_TYPE;

typedef enum {
    EVENT_OUTPUTS_DONE = 0,
    EVENT_TRIPPED,
    EVENT_ERROR
} EVENT_TYPE;

typedef struct {
    guint8* data;
    guint32  fd;
    guint32 meta_fd;
    guint32 size;
    guint32 capacity;       ///< Total allocation size
    guint32 offset;
    guint64 timestamp;
    guint64 index;
    FLAG_TYPE flag;
} BufferDescriptor;

typedef struct {
    gboolean isInput;
    union{
        guint32 u32;
        guint32 u64;
        gint32 i32;
        gint64 i64;
    } val;

    struct {
        guint32 width;
        guint32 height;
    } resolution;

    union{
        PIXEL_FORMAT_TYPE fmt;
    } pixelFormat;

    union{
        INTERLACE_MODE_TYPE type;
    } interlaceMode;
} ConfigParams;

typedef void (*listener_cb)(const void* handle, EVENT_TYPE type, void* data);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Component Store API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* c2componentStore_create();
const gchar* c2componentStore_getName (void* const comp_store);
gboolean c2componentStore_createComponent (void* const comp_store, const gchar* name, void** const component);
gboolean c2componentStore_createInterface (void* const comp_store, const gchar* name, void** const interface);
gboolean c2componentStore_listComponents (void* const comp_store, GPtrArray* array);
gboolean c2componentStore_delete (void* comp_store);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Component API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
gboolean c2component_setListener (void* const comp, void* cb_context, listener_cb callback, BLOCK_MODE_TYPE block);
gboolean c2component_queue(void* const comp, BufferDescriptor* buffer);
gboolean c2component_flush (void* const comp, FLUSH_MODE_TYPE mode, void* const flushedWork);
gboolean c2component_drain (void* const comp, DRAIN_MODE_TYPE mode);
gboolean c2component_start (void* const comp);
gboolean c2component_stop (void* const comp); 
gboolean c2component_reset (void* const comp);
gboolean c2component_release (void* const comp);
void* c2component_intf (void* const comp);
gboolean c2component_createBlockpool (void* const comp, BUFFER_POOL_TYPE poolType);
gboolean c2component_mapOutBuffer (void* const comp, gboolean map);
gboolean c2component_freeOutBuffer (void* const comp, guint64 bufferId);
gboolean c2component_set_pool_property (void* comp, BUFFER_POOL_TYPE poolType, guint32 width,
                                        guint32 height, PIXEL_FORMAT_TYPE fmt);
gboolean c2component_delete (void* comp);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ComponentInterface API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const gchar* c2componentInterface_getName (void* const comp_intf);
const gint c2componentInterface_getId (void* const comp_intf);
gboolean c2componentInterface_config (void* const comp_intf, GHashTable* config, BLOCK_MODE_TYPE block);
gboolean c2componentInterface_delete (void* comp_intf);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
guint32 get_output_frame_size(guint32 width, guint32 height, PIXEL_FORMAT_TYPE fmt);

#ifdef __cplusplus
}
#endif

#endif /* __CODEC2WRAPPER_H__ */
