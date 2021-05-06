/*--------------------------------------------------------------------------
Copyright (c) 2020, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--------------------------------------------------------------------------*/

/*============================================================================
                            O p e n M A X   w r a p p e r s
                             O p e n  M A X   C o r e

  This module contains the implementation of the OpenMAX core & component.

*//*========================================================================*/

#ifndef VDEC_OMX_SAMPLE_VIDEO_TYPES_H_
#define VDEC_OMX_SAMPLE_VIDEO_TYPES_H_

#include "OMX_Video.h"
#include "OMX_QCOMExtns.h"
#include <glib.h>

#define H264_START_CODE 0x00000001
#define H265_START_CODE 0x00000001
#define VOP_START_CODE 0x000001B6
#define SHORT_HEADER_START_CODE 0x00008000
#define MPEG2_FRAME_START_CODE 0x00000100
#define MPEG2_SEQ_START_CODE 0x000001B3
#define VC1_START_CODE  0x00000100
#define VC1_FRAME_START_CODE  0x0000010D
#define VC1_FRAME_FIELD_CODE  0x0000010C
#define VC1_SEQUENCE_START_CODE 0x0000010F
#define VC1_ENTRY_POINT_START_CODE 0x0000010E
#define NUMBER_OF_ARBITRARYBYTES_READ  (4 * 1024)
#define VC1_SEQ_LAYER_SIZE_WITHOUT_STRUCTC 32
#define VC1_SEQ_LAYER_SIZE_V1_WITHOUT_STRUCTC 16
#define HEVC_NAL_UNIT_TYPE_MASK 0x7F
#define HEVC_NAL_UNIT_TYPE_TRAIL_N 0x00
#define HEVC_NAL_UNIT_TYPE_NONIDR 0x16
#define HEVC_NAL_UNIT_TYPE_IDR_W_RADL 19
#define HEVC_NAL_UNIT_TYPE_IDR_N_LP 20
#define HEVC_NAL_UNIT_TYPE_RSV_VCL_N10 10
#define HEVC_NAL_UNIT_TYPE_RSV_VCL_N12 12
#define HEVC_NAL_UNIT_TYPE_RSV_VCL_N14 14
#define HEVC_NAL_UNIT_TYPE_RSV_VCL_R11 11
#define HEVC_NAL_UNIT_TYPE_RSV_VCL_R13 13
#define HEVC_NAL_UNIT_TYPE_RSV_VCL_R15 15
#define HEVC_NAL_UNIT_TYPE_RSV_IRAP_VCL22 22
#define HEVC_NAL_UNIT_TYPE_RSV_IRAP_VCL23 23
#define HEVC_NAL_UNIT_TYPE_VCL_LIMIT 23
#define HEVC_NAL_UNIT_TYPE_RESERVED_START 0x18
#define HEVC_NAL_UNIT_TYPE_RESERVED_END 0x1F
#define HEVC_NAL_UNIT_TYPE_VPS 0x20
#define HEVC_NAL_UNIT_TYPE_SPS 0x21
#define HEVC_NAL_UNIT_TYPE_PPS 0x22
#define HEVC_NAL_UNIT_TYPE_AUD 0x23
#define HEVC_NAL_UNIT_TYPE_PREFIX_SEI 0x27
#define HEVC_NAL_UNIT_TYPE_SUFFIX_SEI 0x28
#define HEVC_NAL_UNIT_TYPE_RESERVED_UNSPECIFIED 0x29
#define HEVC_FIRST_MB_IN_SLICE_MASK 0x80

#define SIZE_NAL_FIELD_MAX  4

#define strlcpy g_strlcpy

// log control
extern uint32_t debug_level_sets;

enum Mode
{
  MODE_PROFILE,      // read some frames to memory, cycle decode these frames
  MODE_FILE_DECODE,  // read data from file
};

enum PortIndexType
{
  PORT_INDEX_IN = 0,
  PORT_INDEX_OUT = 1,
  PORT_EXTRADATA_IN = 2,
  PORT_EXTRADATA_OUT = 3,
  PORT_INDEX_ALL = 0xFFFFFFFF,
};

typedef enum {
  FILE_TYPE_DAT_PER_AU = 1,
  FILE_TYPE_ARBITRARY_BYTES,
  FILE_TYPE_COMMON_CODEC_MAX,

  FILE_TYPE_START_OF_H264_SPECIFIC = 10,
  FILE_TYPE_264_NAL_SIZE_LENGTH = FILE_TYPE_START_OF_H264_SPECIFIC,
  FILE_TYPE_264_START_CODE_BASED,

  FILE_TYPE_START_OF_MP4_SPECIFIC = 20,
  FILE_TYPE_PICTURE_START_CODE = FILE_TYPE_START_OF_MP4_SPECIFIC,

  FILE_TYPE_START_OF_VC1_SPECIFIC = 30,
  FILE_TYPE_RCV = FILE_TYPE_START_OF_VC1_SPECIFIC,
  FILE_TYPE_VC1,

  FILE_TYPE_START_OF_DIVX_SPECIFIC = 40,
  FILE_TYPE_DIVX_4_5_6 = FILE_TYPE_START_OF_DIVX_SPECIFIC,
  FILE_TYPE_DIVX_311,

  FILE_TYPE_START_OF_MPEG2_SPECIFIC = 50,
  FILE_TYPE_MPEG2_START_CODE = FILE_TYPE_START_OF_MPEG2_SPECIFIC,

  FILE_TYPE_START_OF_VP8_SPECIFIC = 60,
  FILE_TYPE_VP8_START_CODE = FILE_TYPE_START_OF_VP8_SPECIFIC,
  FILE_TYPE_VP8,

  FILE_TYPE_START_OF_H265_SPECIFIC = 70,
  FILE_TYPE_265_NAL_SIZE_LENGTH = FILE_TYPE_START_OF_H265_SPECIFIC,
  FILE_TYPE_265_START_CODE_BASED

} FileType;

typedef struct VideoCodecSetting {
  uint32_t eCodec;  // OMX_VIDEO_CODINGTYPE
  const char * sCodecName;
  uint32_t nColorFormat;
  const char * sColorName;
  uint32_t nFileType;

  uint32_t nFrameWidth;
  uint32_t nFrameHeight;
  uint32_t nFrameRate;
  uint32_t nTimestampInterval;
  uint32_t nPictureOrder;
  uint32_t nNalSize;
  
  uint32_t nRectangleLeft;
  uint32_t nRectangleTop;
  uint32_t nRectangleRight;
  uint32_t nRectangleBottom;

} VideoCodecSetting_t;

int32_t FindCodecTypeByName(char * name);
const char * FindCodecNameByType(int32_t type);
const char * FindColorNameByOmxFmt(int32_t omx_fmt_value);
const int32_t FindOmxFmtByColorName(char * name);
const char * GetComponentRole(int32_t codec, int32_t filetype);
const int32_t UpdateFileType(int32_t filetype, int32_t codec);
int32_t GetFramePackingFormat(int32_t filetype);


#endif  // VDEC_OMX_SAMPLE_VIDEO_TYPES_H_
