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

#include <vidc/media/msm_media_info.h>

#include "video_types.h"
#include "video_test_debug.h"

struct codec_map {
    char str[16];
    int32_t value;
};

struct codec_map m_sCodecMaps[10] = {
    {"H264",     OMX_VIDEO_CodingAVC},
    {"MPEG4",    OMX_VIDEO_CodingMPEG4},
    {"H263",     OMX_VIDEO_CodingH263},
    {"WMV",      OMX_VIDEO_CodingWMV},
    {"DIVX",     QOMX_VIDEO_CodingDivx},
    {"MPEG2",    OMX_VIDEO_CodingMPEG2},
    {"VP8",      OMX_VIDEO_CodingVP8},
    {"VP9",      OMX_VIDEO_CodingVP9},
    //{"HEIC",     OMX_VIDEO_CodingImageHEIC},
    {"HEIC",     OMX_VIDEO_CodingHEVC},
    {"HEVC",     OMX_VIDEO_CodingHEVC}
};

int32_t FindCodecTypeByName(char * name)
{
  int32_t type = -1;

  FUNCTION_ENTER();
  if (name == NULL)
  {
    VLOGE("invalid parameter");
    FUNCTION_EXIT();
    return type;
  }

  for (int i=0; i < sizeof(m_sCodecMaps) / sizeof(codec_map); i++)
  {
    if (!strncasecmp(name, m_sCodecMaps[i].str,
          sizeof(m_sCodecMaps[i].str)))
    {
      type = m_sCodecMaps[i].value;
      break;
    }
  }

  FUNCTION_EXIT();
  return type;
}

const char * FindCodecNameByType(int32_t type)
{
  char * name = NULL;

  FUNCTION_ENTER();
  for (int i=0; i < sizeof(m_sCodecMaps) / sizeof(codec_map); i++)
  {
    if (m_sCodecMaps[i].value == type)
    {
      name = m_sCodecMaps[i].str;
      break;
    }
  }

  FUNCTION_EXIT();
  return name;
}

struct format_map {
  char str[16];
  // refer to: enum color_fmts
  int32_t color_fmt_value;  // used by kernel, compute align

  // refer to: enum OMX_COLOR_FORMATTYPE or enum QOMX_COLOR_FORMATTYPE
  int32_t omx_fmt_value; // used by OMX component or hal
};

struct format_map m_FormatMaps[] = {
  {"NV12", COLOR_FMT_NV12, QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m},
  // NV12_UBWC_8bit
  {"NV12_UBWC", COLOR_FMT_NV12_UBWC, QOMX_COLOR_FORMATYUV420PackedSemiPlanar32mCompressed},
  // NOT SUPPORTED
  //{"P010", COLOR_FMT_P010, QOMX_COLOR_FORMATYUV420SemiPlanarP010Venus},
  // NV12_UBWC_10bit
  {"TP10_UBWC", COLOR_FMT_NV12_BPP10_UBWC, QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m10bitCompressed},
};

const char * FindColorNameByOmxFmt(int32_t omx_fmt_value)
{
  char * name = NULL;
  FUNCTION_ENTER();
  for (int i=0; i < sizeof(m_FormatMaps) / sizeof(struct format_map); i++)
  {
    if (m_FormatMaps[i].omx_fmt_value == omx_fmt_value)
    {
      name = m_FormatMaps[i].str;
      break;
    }
  }
  FUNCTION_EXIT();
  return name;
}

const int32_t FindOmxFmtByColorName(char * name)
{
  int32_t omx_fmt = 0;
  FUNCTION_ENTER();
  for (int i=0; i < sizeof(m_FormatMaps) / sizeof(struct format_map); i++)
  {
    if (!strcmp(m_FormatMaps[i].str, name))
    {
      omx_fmt = m_FormatMaps[i].omx_fmt_value;
      break;
    }
  }
  FUNCTION_EXIT();
  return omx_fmt;
}

struct codec_file_map {
  char str[32];
  // refer to: enum file types
  int32_t file_fmt_value;
  // refer to: enum OMX_CODEC_FORMATTYPE or enum QOMX_CODEC_FORMATTYPE
  int32_t codec_fmt_value;
};

struct codec_file_map m_sCodecFileMaps[18] = {
  {"OMX.qcom.video.decoder.avc", FILE_TYPE_264_NAL_SIZE_LENGTH, OMX_VIDEO_CodingAVC},
  {"OMX.qcom.video.decoder.avc", FILE_TYPE_ARBITRARY_BYTES, OMX_VIDEO_CodingAVC},
  {"OMX.qcom.video.decoder.avc", FILE_TYPE_264_START_CODE_BASED, OMX_VIDEO_CodingAVC},
  {"OMX.qcom.video.decoder.vp8", FILE_TYPE_VP8, OMX_VIDEO_CodingVP8},
  {"OMX.qcom.video.decoder.vp9", FILE_TYPE_VP8, OMX_VIDEO_CodingVP9},
  {"OMX.qcom.video.decoder.hevc", FILE_TYPE_265_START_CODE_BASED, OMX_VIDEO_CodingHEVC},
  //{"OMX.qcom.video.decoder.heic", FILE_TYPE_265_START_CODE_BASED, OMX_VIDEO_CodingImageHEIC},
  {"OMX.qcom.video.decoder.mpeg2", FILE_TYPE_ARBITRARY_BYTES, OMX_VIDEO_CodingMPEG2},
  {"OMX.qcom.video.decoder.mpeg2", FILE_TYPE_MPEG2_START_CODE, OMX_VIDEO_CodingMPEG2},
#ifdef DISABLE_SW_VDEC
  {"OMX.qcom.video.decoder.mpeg4", FILE_TYPE_ARBITRARY_BYTES, OMX_VIDEO_CodingMPEG4},
  {"OMX.qcom.video.decoder.mpeg4", FILE_TYPE_PICTURE_START_CODE, OMX_VIDEO_CodingMPEG4},
  {"OMX.qcom.video.decoder.h263", FILE_TYPE_ARBITRARY_BYTES, OMX_VIDEO_CodingH263},
  {"OMX.qcom.video.decoder.h263", FILE_TYPE_PICTURE_START_CODE, OMX_VIDEO_CodingH263},
  {"OMX.qcom.video.decoder.vc1", FILE_TYPE_ARBITRARY_BYTES, OMX_VIDEO_CodingWMV},
  {"OMX.qcom.video.decoder.vc1", FILE_TYPE_VC1, OMX_VIDEO_CodingWMV},
  {"OMX.qcom.video.decoder.wmv", FILE_TYPE_ARBITRARY_BYTES, OMX_VIDEO_CodingWMV},
  {"OMX.qcom.video.decoder.wmv", FILE_TYPE_RCV, OMX_VIDEO_CodingWMV},
  {"OMX.qcom.video.decoder.divx", FILE_TYPE_DIVX_4_5_6, OMX_VIDEO_CodingDivx},
  {"OMX.qcom.video.decoder.divx311", FILE_TYPE_DIVX_311, OMX_VIDEO_CodingDivx},
#else
  {"OMX.qti.video.decoder.mpeg4sw", FILE_TYPE_PICTURE_START_CODE, OMX_VIDEO_CodingMPEG4},
  {"OMX.qti.video.decoder.mpeg4sw", FILE_TYPE_PICTURE_START_CODE, OMX_VIDEO_CodingMPEG4},
  {"OMX.qti.video.decoder.h263sw", FILE_TYPE_PICTURE_START_CODE, OMX_VIDEO_CodingH263},
  {"OMX.qti.video.decoder.h263sw", FILE_TYPE_PICTURE_START_CODE, OMX_VIDEO_CodingH263},
  {"OMX.qti.video.decoder.vc1sw", FILE_TYPE_ARBITRARY_BYTES, OMX_VIDEO_CodingWMV},
  {"OMX.qti.video.decoder.vc1sw", FILE_TYPE_VC1, OMX_VIDEO_CodingWMV},
  {"OMX.qti.video.decoder.vc1sw", FILE_TYPE_ARBITRARY_BYTES, OMX_VIDEO_CodingWMV},
  {"OMX.qti.video.decoder.vc1sw", FILE_TYPE_RCV, OMX_VIDEO_CodingWMV},
  {"OMX.qti.video.decoder.divx4sw", FILE_TYPE_DIVX_4_5_6, QOMX_VIDEO_CodingDivx},
  {"OMX.qti.video.decoder.divxsw", FILE_TYPE_DIVX_311, QOMX_VIDEO_CodingDivx},
#endif
};

const char * GetComponentRole(int32_t codec, int32_t filetype)
{
  char * name = NULL;
  FUNCTION_ENTER();
  for (int i=0; i < sizeof(m_sCodecFileMaps) / sizeof(struct codec_file_map); i++)
  {
    if (m_sCodecFileMaps[i].codec_fmt_value == codec)
    {
      if (filetype < FILE_TYPE_COMMON_CODEC_MAX || m_sCodecFileMaps[i].file_fmt_value == filetype)
      {
        name = m_sCodecFileMaps[i].str;
        break;
      }
    }
  }
  FUNCTION_EXIT();
  return name;
}

const int32_t UpdateFileType(int32_t filetype, int32_t codec)
{
  int32_t file_type = filetype;
  FUNCTION_ENTER();
  switch (codec)
  {
    case OMX_VIDEO_CodingAVC:
      file_type = FILE_TYPE_START_OF_H264_SPECIFIC + filetype - FILE_TYPE_COMMON_CODEC_MAX;
      break;
    case QOMX_VIDEO_CodingDivx:
      file_type = FILE_TYPE_START_OF_DIVX_SPECIFIC + filetype - FILE_TYPE_COMMON_CODEC_MAX;
      break;
    case OMX_VIDEO_CodingMPEG4:
    case OMX_VIDEO_CodingH263:
      file_type = FILE_TYPE_START_OF_MP4_SPECIFIC + filetype - FILE_TYPE_COMMON_CODEC_MAX;
      break;
    case OMX_VIDEO_CodingWMV:
      file_type = FILE_TYPE_START_OF_VC1_SPECIFIC + filetype - FILE_TYPE_COMMON_CODEC_MAX;
      break;
    case OMX_VIDEO_CodingMPEG2:
      file_type = FILE_TYPE_START_OF_MPEG2_SPECIFIC + filetype - FILE_TYPE_COMMON_CODEC_MAX;
      break;
    case OMX_VIDEO_CodingVP8:
    case OMX_VIDEO_CodingVP9:
      file_type = FILE_TYPE_START_OF_VP8_SPECIFIC + filetype - FILE_TYPE_COMMON_CODEC_MAX;
      break;
    case OMX_VIDEO_CodingHEVC:
    //case OMX_VIDEO_CodingImageHEIC:
      file_type = FILE_TYPE_START_OF_H265_SPECIFIC + filetype - FILE_TYPE_COMMON_CODEC_MAX;
      break;
    default:
      VLOGE("Error: Unknown code %d", codec);
  }
  FUNCTION_EXIT();
  return file_type;
}

struct framepack_map {
  // refer to: enum file types
  int32_t file_fmt_value;
  // erfer to: enum FramePackingFormat
  int32_t packing_fmt_value;
};

struct framepack_map m_FramePackingMaps[12] = {
  {FILE_TYPE_DAT_PER_AU, OMX_QCOM_FramePacking_OnlyOneCompleteFrame},
  {FILE_TYPE_PICTURE_START_CODE, OMX_QCOM_FramePacking_OnlyOneCompleteFrame},
  {FILE_TYPE_MPEG2_START_CODE, OMX_QCOM_FramePacking_OnlyOneCompleteFrame},
  {FILE_TYPE_264_START_CODE_BASED, OMX_QCOM_FramePacking_OnlyOneCompleteFrame},
  {FILE_TYPE_RCV, OMX_QCOM_FramePacking_OnlyOneCompleteFrame},
  {FILE_TYPE_VC1, OMX_QCOM_FramePacking_OnlyOneCompleteFrame},
  {FILE_TYPE_DIVX_311, OMX_QCOM_FramePacking_OnlyOneCompleteFrame},
  {FILE_TYPE_ARBITRARY_BYTES, OMX_QCOM_FramePacking_Arbitrary},
  {FILE_TYPE_264_NAL_SIZE_LENGTH, OMX_QCOM_FramePacking_Arbitrary},
  {FILE_TYPE_DIVX_4_5_6, OMX_QCOM_FramePacking_Arbitrary},
  {FILE_TYPE_VP8, OMX_QCOM_FramePacking_OnlyOneCompleteFrame},
  {FILE_TYPE_265_START_CODE_BASED, OMX_QCOM_FramePacking_OnlyOneCompleteFrame},
};

int32_t GetFramePackingFormat(int32_t filetype)
{
  FUNCTION_ENTER();
  int32_t frame_packing_format = OMX_QCOM_FramePacking_Unspecified;
  for (int i=0; i < sizeof(m_FramePackingMaps) / sizeof(struct framepack_map); i++)
  {
    if (m_FramePackingMaps[i].file_fmt_value == filetype)
    {
      frame_packing_format = m_FramePackingMaps[i].packing_fmt_value;
      break;
    }
  }
  FUNCTION_EXIT();
  return frame_packing_format;
}
