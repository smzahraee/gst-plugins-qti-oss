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

#ifndef VENC_OMX_SAMPLE_VIDEO_TYPES_H_
#define VENC_OMX_SAMPLE_VIDEO_TYPES_H_

#include "OMX_Video.h"
#include "OMX_QCOMExtns.h"
#include <cutils/properties.h>
#include <glib.h>

#define MAX_STR_LEN 256

#define ALIGN(__sz, __align) (((__align) & ((__align) - 1)) ?\
    ((((__sz) + (__align) - 1) / (__align)) * (__align)) :\
    (((__sz) + (__align) - 1) & (~((__align) - 1))))

#define strlcpy g_strlcpy

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef memscpy
#define memscpy(dst, dst_size, src, bytes_to_copy) (void) \
                    memcpy(dst, src, MIN(dst_size, bytes_to_copy))
#endif

enum PortIndexType {
  PORT_INDEX_IN = 0,
  PORT_INDEX_OUT = 1,
  PORT_INDEX_ALL = 0xFFFFFFFF,
};

enum ResyncMarkerType {
  RESYNC_MARKER_NONE = 0,    ///< No resync marker
  RESYNC_MARKER_MB,    ///< MB resync marker for MPEG4, H.264, H263
  RESYNC_MARKER_BYTE,    ///< BYTE Resync marker for MPEG4, H.264, H263
  RESYNC_MARKER_GOB      ///< GOB resync marker for H.263
};

enum Mode {
  MODE_PREVIEW,
  MODE_DISPLAY,
  MODE_PROFILE,      // read some YUV frame to memory, cycle encode these frames
  MODE_FILE_ENCODE,  // read YUV data from file
  MODE_LIVE_ENCODE
};

typedef struct VideoCodecSetting {
  uint32_t eCodec;  // OMX_VIDEO_CODINGTYPE
  int32_t eLevel;  // OMX_VIDEO_AVCLEVELTYPE
  int32_t eControlRate;  // OMX_VIDEO_CONTROLRATETYPE
  uint32_t eSliceMode;  // OMX_VIDEO_AVCSLICEMODETYPE
  uint32_t nFrameWidth;
  uint32_t nFrameHeight;
  uint32_t nScalingWidth;
  uint32_t nScalingHeight;
  uint32_t nRectangleLeft;
  uint32_t nRectangleTop;
  uint32_t nRectangleRight;
  uint32_t nRectangleBottom;
  uint32_t nFrameBytes;

  uint32_t nFramestride;
  uint32_t nFrameScanlines;
  uint32_t nFrameRead;

  uint32_t nBitRate;
  uint32_t nFrameRate;
  uint32_t nFormat;
  uint32_t nInputBufferCount;
  uint32_t nInputBufferSize;
  uint32_t nOutputBufferCount;
  uint32_t nOutputBufferSize;
  int32_t nUserProfile;  // enum
  int32_t bPrependSPSPPSToIDRFrame;  // OMX_BOOL == enum
  int32_t bAuDelimiters;  // OMX_BOOL == enum
  uint32_t nIDRPeriod;
  uint32_t nPFrames;
  uint32_t nBFrames;

  uint32_t eResyncMarkerType;
  uint32_t nResyncMarkerSpacing;
  uint32_t nHECInterval;

  int32_t nRefreshMode;
  uint32_t nIntraRefresh;
  char * configFileName;

  uint32_t nRotation;
  uint32_t nFrames;
  uint32_t nMirror;
} VideoCodecSetting_t;

union DynamicConfigData {
  OMX_VIDEO_CONFIG_BITRATETYPE bitrate;
  OMX_CONFIG_FRAMERATETYPE framerate;
  QOMX_VIDEO_INTRAPERIODTYPE intraperiod;
};

typedef struct DynamicConfigure {
  unsigned frame_num;
  OMX_INDEXTYPE config_param;
  union DynamicConfigData config_data;
} DynamicConfigure_t;

int32_t FindCodecTypeByName(char * name);
const char * FindCodecNameByType(int32_t type);
const char * FindColorNameByOmxFmt(int32_t omx_fmt_value);
const int32_t FindOmxFmtByColorName(char * name);
const int32_t FindIntraRefreshModeByName(char * name);
const int32_t FindResyncMarkerTypeByName(char * name);
uint32_t ConvertColorFomat(uint32_t videoFormat);
uint32_t ConvertColorToGbmFormat(uint32_t video_format);
const char * ConvertEcodeToStr(OMX_U32 codec);

#endif  // VENC_OMX_SAMPLE_VIDEO_TYPES_H_
