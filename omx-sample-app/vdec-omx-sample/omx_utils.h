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

#ifndef VDEC_OMX_SAMPLE_OMX_UTILS_H_
#define VDEC_OMX_SAMPLE_OMX_UTILS_H_

#include <time.h>
#include "video_types.h"

// log control
extern uint32_t debug_level_sets;

// test mode
extern int32_t m_TestMode;

extern char m_OutputFileName[128];
extern char m_InputFileName[128];

extern int64_t m_DecodeTotalTimeActal;
extern int64_t m_DecodeFrameTimeMax;
extern int64_t m_DecodeFrameTimeMin;
extern int32_t m_InputFrameNum;
extern int32_t m_OutputFrameNum;
extern int32_t m_InputDataSize;
extern int32_t m_OutputDataSize;

extern timeval m_DecodeStartTime;
extern timeval m_DecodeEndTime;


//static int open_video_file();
bool SetReadBufferType(int32_t file_type_option, int32_t codec_format_option);
bool InitializeCodec(OMX_U32 eCodec, int32_t filetype);
bool ConfigureCodec(VideoCodecSetting_t *codecSettings);
bool StartDecoder();
void WaitingForTestDone();
bool StopDecoder();
bool ReleaseBuffers(PortIndexType port);
void ReleaseResource();
bool DisableOutputPort();
bool EnableOutputPort();
bool ReconfigOutputPort();
void CheckIsSWCodec(const char * component);

#endif  // VDEC_OMX_SAMPLE_OMX_UTILS_H_
