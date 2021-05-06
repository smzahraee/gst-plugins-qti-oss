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

//////////////////////////////////////////////////////////////////////////////
//                             Include Files
//////////////////////////////////////////////////////////////////////////////

#include <vidc/media/msm_media_info.h>

#include "video_types.h"
#include "video_debug.h"
#include "gbm.h"

extern uint32_t m_DebugLevelSets;

struct CodecMap {
  char str[16];
  int32_t value;
};

struct CodecMap m_sCodecMaps[6] = {
  {"MPEG4",   OMX_VIDEO_CodingMPEG4},
  {"H263",    OMX_VIDEO_CodingH263},
  {"H264",    OMX_VIDEO_CodingAVC},
  {"VP8",     OMX_VIDEO_CodingVP8},
  {"HEVC",    OMX_VIDEO_CodingHEVC},
  {"HEIC",    OMX_VIDEO_CodingImageHEIC},
};

struct FormatMap {
  char str[16];
  // refer to: enum color_fmts
  int32_t color_fmt_value;  // used by kernel, compute align

  // refer to: enum OMX_COLOR_FORMATTYPE or enum QOMX_COLOR_FORMATTYPE
  int32_t omx_fmt_value;  // used by OMX component or hal
};

struct FormatMap m_FormatMaps[] = {
  {"NV12", COLOR_FMT_NV12, QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m},
  // NV12_UBWC_8bit
  {"NV12_UBWC", COLOR_FMT_NV12_UBWC, QOMX_COLOR_FORMATYUV420PackedSemiPlanar32mCompressed},
  // P010_10bit
  {"P010", COLOR_FMT_P010, QOMX_COLOR_FORMATYUV420SemiPlanarP010Venus},
  // NV12_UBWC_10bit
  {"TP10_UBWC", COLOR_FMT_NV12_BPP10_UBWC, QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m10bitCompressed},
};

struct IntrarefreshModeMap {
  char str[16];
  // refer to: enum OMX_VIDEO_INTRAREFRESHTYPE
  int32_t omx_intrarefresh_mode;  // used by OMX component or hal
};

struct IntrarefreshModeMap m_IntraRefreshModeMaps[] = {
  {"IR_CYCLIC", OMX_VIDEO_IntraRefreshCyclic},
  {"IR_RANDOM", OMX_VIDEO_IntraRefreshRandom},
  {"IR_ADAPTIVE", OMX_VIDEO_IntraRefreshAdaptive},
};

struct ResyncmarkerTypeMap {
  char str[16];
  // refer to enum ResyncMarkerType
  int32_t resyncmarker_type;
};

struct ResyncmarkerTypeMap m_ResyncMarkerTypeMaps[] = {
  {"MB", RESYNC_MARKER_MB},
  {"BYTE", RESYNC_MARKER_BYTE},
  {"GOB", RESYNC_MARKER_GOB},
};

int32_t FindCodecTypeByName(char * name) {
  int32_t type = -1;

  if (name == NULL) {
    VLOGE("invalid parameter");
    FUNCTION_EXIT();
    return type;
  }

  for (int i=0; i < sizeof(m_sCodecMaps) / sizeof(CodecMap); i++) {
    if (!strncasecmp(name, m_sCodecMaps[i].str,
          sizeof(m_sCodecMaps[i].str))) {
      type = m_sCodecMaps[i].value;
      break;
    }
  }

  return type;
}

const char * FindCodecNameByType(int32_t type) {
  char * name = NULL;

  for (int i=0; i < sizeof(m_sCodecMaps) / sizeof(CodecMap); i++) {
    if (m_sCodecMaps[i].value == type) {
      name = m_sCodecMaps[i].str;
      break;
    }
  }

  return name;
}

const char * FindColorNameByOmxFmt(int32_t omx_fmt_value) {
  char * name = NULL;

  for (int i=0; i < sizeof(m_FormatMaps) / sizeof(struct FormatMap); i++) {
    if (m_FormatMaps[i].omx_fmt_value == omx_fmt_value) {
      name = m_FormatMaps[i].str;
      break;
    }
  }

  return name;
}

const int32_t FindOmxFmtByColorName(char * name) {
  int32_t omx_fmt = 0;
  for (int i=0; i < sizeof(m_FormatMaps) / sizeof(struct FormatMap); i++) {
    if (!strncasecmp(name, m_FormatMaps[i].str, sizeof(m_FormatMaps[i].str))) {
      omx_fmt = m_FormatMaps[i].omx_fmt_value;
      break;
    }
  }
  return omx_fmt;
}

const int32_t FindIntraRefreshModeByName(char * name) {
  int32_t refresh_mode = 0;
  for (int i=0; i < sizeof(m_IntraRefreshModeMaps) / sizeof(struct IntrarefreshModeMap); i++) {
    if (!strncasecmp(name, m_IntraRefreshModeMaps[i].str, sizeof(m_IntraRefreshModeMaps[i].str))) {
      refresh_mode = m_IntraRefreshModeMaps[i].omx_intrarefresh_mode;
      break;
    }
  }
  return refresh_mode;
}

const int32_t FindResyncMarkerTypeByName(char * name) {
  int32_t resync_marker_type = 0;
  for (int i=0; i < sizeof(m_ResyncMarkerTypeMaps) / sizeof(struct ResyncmarkerTypeMap); i++) {
    if (!strncasecmp(name, m_ResyncMarkerTypeMaps[i].str, sizeof(m_ResyncMarkerTypeMaps[i].str))) {
      resync_marker_type = m_ResyncMarkerTypeMaps[i].resyncmarker_type;
      break;
    }
  }
  return resync_marker_type;
}

// convert video encode format to kernel format
uint32_t ConvertColorFomat(uint32_t video_format) {
  uint32_t color_format = 0;

  FUNCTION_ENTER();

  switch (video_format) {
    case QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m:
      color_format = COLOR_FMT_NV12;
      break;
    case QOMX_COLOR_FORMATYUV420PackedSemiPlanar32mCompressed:
      color_format = COLOR_FMT_NV12_UBWC;
      break;
    case QOMX_COLOR_Format32bitRGBA8888:
      color_format = COLOR_FMT_RGBA8888;
      break;
    case QOMX_COLOR_Format32bitRGBA8888Compressed:
      color_format = COLOR_FMT_RGBA8888_UBWC;
      break;
    case QOMX_COLOR_FORMATYUV420SemiPlanarP010Venus:
      color_format = COLOR_FMT_P010;
      break;
    case QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m10bitCompressed:
      color_format = COLOR_FMT_NV12_BPP10_UBWC;
      break;
    default:
      VLOGE("Invalid videoFormat: 0x%x", video_format);
      break;
  }

  FUNCTION_EXIT();

  return color_format;
}

// convert video kernel format to color format
uint32_t ConvertColorToGbmFormat(uint32_t video_format) {
  uint32_t gbm_format = 0;

  FUNCTION_ENTER();

  switch (video_format) {
    case QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m:
    case QOMX_COLOR_FORMATYUV420PackedSemiPlanar32mCompressed:
      gbm_format = GBM_FORMAT_NV12;
      break;
    case QOMX_COLOR_Format32bitRGBA8888:
    case QOMX_COLOR_Format32bitRGBA8888Compressed:
      gbm_format = GBM_FORMAT_RGB888;
      break;
    case QOMX_COLOR_FORMATYUV420SemiPlanarP010Venus:
    case QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m10bitCompressed:
      gbm_format = GBM_FORMAT_UYVY;
      break;
    default:
      VLOGE("Invalid colorFormat: 0x%x", video_format);
      break;
  }

  FUNCTION_EXIT();

  return gbm_format;
}

const char * ConvertEcodeToStr(OMX_U32 codec) {
  switch (codec) {
    case OMX_VIDEO_CodingAutoDetect:
      return "AutoDetect";
    case OMX_VIDEO_CodingMPEG2:
      return "MPEG2";
    case OMX_VIDEO_CodingH263:
      return "H263";
    case OMX_VIDEO_CodingMPEG4:
      return "MPEG4";
    case OMX_VIDEO_CodingWMV:
      return "WMV";
    case OMX_VIDEO_CodingRV:
      return "RV";
    case OMX_VIDEO_CodingAVC:
      return "AVC/H264";
    case OMX_VIDEO_CodingMJPEG:
      return "MJPEG";
    case OMX_VIDEO_CodingVP8:
      return "VP8";
    case OMX_VIDEO_CodingVP9:
      return "VP9";
    case OMX_VIDEO_CodingHEVC:
      return "HEVC";
    case OMX_VIDEO_CodingDolbyVision:
      return "DoblyVision";
    case OMX_VIDEO_CodingImageHEIC:
      return "HEIF image encoded with HEVC";
    default:
      return "Unkown codec";
  }
}
