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
#include <dlfcn.h>
#include <gst/video/video.h>

#define ALIGN(num, to) (((num) + (to - 1)) & (~(to - 1)))

#define CONFIG_FUNCTION_KEY_PIXELFORMAT "pixelformat"
#define CONFIG_FUNCTION_KEY_RESOLUTION "resolution"
#define CONFIG_FUNCTION_KEY_BITRATE "bitrate"
#define CONFIG_FUNCTION_KEY_INTERLACE "interlace"
#define CONFIG_FUNCTION_KEY_MIRROR "mirror"
#define CONFIG_FUNCTION_KEY_ROTATION "rotation"
#define CONFIG_FUNCTION_KEY_RATECONTROL "ratecontrol"
#define CONFIG_FUNCTION_KEY_DEC_LOW_LATENCY "dec_low_latency"
#define CONFIG_FUNCTION_KEY_INTRAREFRESH "intra_refresh"
#define CONFIG_FUNCTION_KEY_OUTPUT_PICTURE_ORDER_MODE "output_picture_order_mode"
#define CONFIG_FUNCTION_KEY_DOWNSCALE "downscale"
#define CONFIG_FUNCTION_KEY_ENC_CSC "enc_colorspace_conversion"
#define CONFIG_FUNCTION_KEY_COLOR_ASPECTS_INFO "colorspace_color_aspects"
#define CONFIG_FUNCTION_KEY_SLICE_MODE "slice_mode"

#define C2_TICKS_PER_SECOND 1000000

typedef enum {
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
    INTERLACE_MODE_PROGRESSIVE = 0, ///< progressive
    INTERLACE_MODE_INTERLEAVED_TOP_FIRST, ///< line-interleaved. top-field-first
    INTERLACE_MODE_INTERLEAVED_BOTTOM_FIRST, ///< line-interleaved. bottom-field-first
    INTERLACE_MODE_FIELD_TOP_FIRST, ///< field-sequential. top-field-first
    INTERLACE_MODE_FIELD_BOTTOM_FIRST, ///< field-sequential. bottom-field-first
} INTERLACE_MODE_TYPE;

typedef enum {
    C2_INTERLACE_MODE_PROGRESSIVE = 0, ///< progressive
    C2_INTERLACE_MODE_INTERLEAVED_TOP_FIRST, ///< line-interleaved. top-field-first
    C2_INTERLACE_MODE_INTERLEAVED_BOTTOM_FIRST, ///< line-interleaved. bottom-field-first
    C2_INTERLACE_MODE_FIELD_TOP_FIRST, ///< field-sequential. top-field-first
    C2_INTERLACE_MODE_FIELD_BOTTOM_FIRST, ///< field-sequential. bottom-field-first
} C2_INTERLACE_MODE_TYPE;

typedef enum {
    FLAG_TYPE_DROP_FRAME = 1 << 0,
    FLAG_TYPE_END_OF_STREAM = 1 << 1, ///< For input frames: no output frame shall be generated when processing this frame.
    ///< For output frames: this frame shall be discarded.
    FLAG_TYPE_DISCARD_FRAME = 1 << 2, ///< This frame shall be discarded with its metadata.
    FLAG_TYPE_INCOMPLETE = 1 << 3, ///< This frame is not the last frame produced for the input
    FLAG_TYPE_CODEC_CONFIG = 1 << 4 ///< Frame contains only codec-specific configuration data, and no actual access unit
} FLAG_TYPE;

typedef enum {
    PIXEL_FORMAT_NV12_LINEAR = 0,
    PIXEL_FORMAT_NV12_UBWC,
    PIXEL_FORMAT_RGBA_8888,
    PIXEL_FORMAT_YV12,
    PIXEL_FORMAT_P010,
    PIXEL_FORMAT_TP10_UBWC
} PIXEL_FORMAT_TYPE;

typedef enum {
    // RGB-Alpha 8 bit per channel
    C2_PIXEL_FORMAT_RGBA8888 = 1,
    // RGBA 8 bit compressed
    C2_PIXEL_FORMAT_RGBA8888_UBWC = 0xC2000000,
    // NV12 EXT with 128 width and height alignment
    C2_PIXEL_FORMAT_VENUS_NV12 = 0x7FA30C04,
    // NV12 EXT with UBWC compression
    C2_PIXEL_FORMAT_VENUS_NV12_UBWC = 0x7FA30C06,
    // 10-bit Tightly-packed and compressed YUV
    C2_PIXEL_FORMAT_VENUS_TP10 = 0x7FA30C09,
    // Venus 10-bit YUV 4:2:0 Planar format
    C2_PIXEL_FORMAT_VENUS_P010 = 0x7FA30C0A,
    ///< canonical YVU 4:2:0 Planar (YV12)
    C2_PIXEL_FORMAT_YV12 = 842094169,
} C2_PIXEL_FORMAT;

typedef enum {
    EVENT_OUTPUTS_DONE = 0,
    EVENT_TRIPPED,
    EVENT_ERROR
} EVENT_TYPE;

typedef enum {
    DEFAULT_ORDER = 0,
    DISPLAY_ORDER,
    DECODER_ORDER,
} OUTPUT_PIC_ORDER;

typedef enum {
    MIRROR_NONE = 0,
    MIRROR_VERTICAL,
    MIRROR_HORIZONTAL,
    MIRROR_BOTH,
} MIRROR_TYPE;

typedef enum {
    RC_OFF = 0,
    RC_CONST,
    RC_CBR_VFR,
    RC_VBR_CFR,
    RC_VBR_VFR,
    RC_CQ,
    RC_UNSET = 0xFFFF
} RC_MODE_TYPE;

typedef enum {
    SLICE_MODE_DISABLE,
    SLICE_MODE_MB,
    SLICE_MODE_BYTES,
} SLICE_MODE;

typedef enum {
    COLOR_PRIMARIES_UNSPECIFIED,
    COLOR_PRIMARIES_BT709,
    COLOR_PRIMARIES_BT470_M,
    COLOR_PRIMARIES_BT601_625,
    COLOR_PRIMARIES_BT601_525,
    COLOR_PRIMARIES_GENERIC_FILM,
    COLOR_PRIMARIES_BT2020,
    COLOR_PRIMARIES_RP431,
    COLOR_PRIMARIES_EG432,
    COLOR_PRIMARIES_EBU3213,
} COLOR_PRIMARIES;

typedef enum {
    COLOR_TRANSFER_UNSPECIFIED,
    COLOR_TRANSFER_LINEAR,
    COLOR_TRANSFER_SRGB,
    COLOR_TRANSFER_170M,
    COLOR_TRANSFER_GAMMA22,
    COLOR_TRANSFER_GAMMA28,
    COLOR_TRANSFER_ST2084,
    COLOR_TRANSFER_HLG,
    COLOR_TRANSFER_240M,
    COLOR_TRANSFER_XVYCC,
    COLOR_TRANSFER_BT1361,
    COLOR_TRANSFER_ST428,
} TRANSFER_CHAR;

typedef enum {
    COLOR_MATRIX_UNSPECIFIED,
    COLOR_MATRIX_BT709,
    COLOR_MATRIX_FCC47_73_682,
    COLOR_MATRIX_BT601,
    COLOR_MATRIX_240M,
    COLOR_MATRIX_BT2020,
    COLOR_MATRIX_BT2020_CONSTANT,
} MATRIX;

typedef enum {
    COLOR_RANGE_UNSPECIFIED,
    COLOR_RANGE_FULL,
    COLOR_RANGE_LIMITED,
} FULL_RANGE;

typedef enum {
    IR_NONE = 0,
    IR_RANDOM,
} IR_MODE_TYPE;

typedef struct {
    guint8* data;
    gint32 fd;
    gint32 meta_fd;
    guint32 size;
    guint32 capacity; ///< Total allocation size
    guint32 offset;
    guint64 timestamp;
    guint64 index;
    guint32 width;
    guint32 height;
    GstVideoFormat format;
    guint32 ubwc_flag;
    FLAG_TYPE flag;
    BUFFER_POOL_TYPE pool_type;
    guint8* config_data; // codec config data
    guint32 config_size; // size of codec config data
    void* c2_buffer;
    void* gbm_bo;
} BufferDescriptor;

typedef struct {
    const char* config_name;
    gboolean isInput;
    union {
        guint32 u32;
        guint64 u64;
        gint32 i32;
        gint64 i64;
    } val;

    struct {
        guint32 width;
        guint32 height;
    } resolution;

    union {
        PIXEL_FORMAT_TYPE fmt;
    } pixelFormat;

    union {
        INTERLACE_MODE_TYPE type;
    } interlaceMode;

    union {
        MIRROR_TYPE type;
    } mirror;

    union {
        RC_MODE_TYPE type;
    } rcMode;

    union {
        SLICE_MODE type;
    } SliceMode;

    struct {
        IR_MODE_TYPE type;
        float intra_refresh_mbs;
    } irMode;
    guint output_picture_order_mode;
    gboolean low_latency_mode;
    gboolean color_space_conversion;
    struct {
        COLOR_PRIMARIES primaries;
        TRANSFER_CHAR transfer_char;
        MATRIX matrix;
        FULL_RANGE full_range;
    } colorAspects;
} ConfigParams;

typedef void (*listener_cb)(const void* handle, EVENT_TYPE type, void* data);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Component Store API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* c2componentStore_create();
const gchar* c2componentStore_getName(void* const comp_store);
gboolean c2componentStore_createComponent(void* const comp_store, const gchar* name, void** const component);
gboolean c2componentStore_createInterface(void* const comp_store, const gchar* name, void** const interface);
gboolean c2componentStore_listComponents(void* const comp_store, GPtrArray* array);
gboolean c2componentStore_isComponentSupported(void* const comp_store, gchar* name);
gboolean c2componentStore_delete(void* comp_store);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Component API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
gboolean c2component_setListener(void* const comp, void* cb_context, listener_cb callback, BLOCK_MODE_TYPE block);
gboolean c2component_alloc(void* const comp, BufferDescriptor* buffer);
gboolean c2component_queue(void* const comp, BufferDescriptor* buffer);
gboolean c2component_flush(void* const comp, FLUSH_MODE_TYPE mode, void* const flushedWork);
gboolean c2component_drain(void* const comp, DRAIN_MODE_TYPE mode);
gboolean c2component_start(void* const comp);
gboolean c2component_stop(void* const comp);
gboolean c2component_reset(void* const comp);
gboolean c2component_release(void* const comp);
void* c2component_intf(void* const comp);
gboolean c2component_createBlockpool(void* const comp, BUFFER_POOL_TYPE poolType);
gboolean c2component_configBlockpool(void* comp, BUFFER_POOL_TYPE poolType);
gboolean c2component_mapOutBuffer(void* const comp, gboolean map);
gboolean c2component_freeOutBuffer(void* const comp, guint64 bufferId);
gboolean c2component_delete(void* comp);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ComponentInterface API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const gchar* c2componentInterface_getName(void* const comp_intf);
const gint c2componentInterface_getId(void* const comp_intf);
gboolean c2componentInterface_config(void* const comp_intf, GPtrArray* config, BLOCK_MODE_TYPE block);

#ifdef __cplusplus
}
#endif

#endif /* __CODEC2WRAPPER_H__ */
