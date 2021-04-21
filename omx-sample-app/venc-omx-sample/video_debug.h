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

#define LOG_TAG "video_sample"

// enable VLOGD
#define DEBUG_LOG

// enable VLOGV
#define VERBOSE_LOG

enum {
  LVL_E = 0x00,
  LVL_P = 0x01,
  LVL_D = 0x02,
  LVL_V = 0x04,
  LVL_F = 0x08
};

/*============================================================================
  Video debug macro define
  ============================================================================*/
// #define __ANDROID__

#ifdef __ANDROID__
#include <cutils/log.h>
// Error log
#define VLOGE(fmt, ...) ALOGE("%s::%d " fmt, __FUNCTION__, __LINE__, ## __VA_ARGS__)

// Performance log
#define VLOGP(fmt, ...) ALOGE("%s::%d " fmt, __FUNCTION__, __LINE__, ## __VA_ARGS__)

// Debug log
#define VLOGD(fmt, ...) ALOGD("%s::%d " fmt, __FUNCTION__, __LINE__, ## __VA_ARGS__)

// Verbose log
#define VLOGV(fmt, ...) ALOGV("%s::%d " fmt, __FUNCTION__, __LINE__, ## __VA_ARGS__)

#else

#include <pthread.h>

#define PRINT_TIME

#ifdef PRINT_TIME
char * GetTimeStamp();
#else
#define GetTimeStamp() " "
#endif

// Error log
#define VLOGE(fmt, ...) fprintf(stderr, LOG_TAG "%s ERROR:%s::%d " fmt "\n", \
    GetTimeStamp(), __FUNCTION__, __LINE__,                   \
## __VA_ARGS__)

// Performance log
#define VLOGP(fmt, ...) if (m_DebugLevelSets & LVL_P) \
  fprintf(stderr, LOG_TAG "%s PERF:%s::%d " fmt "\n",   \
      GetTimeStamp(), __FUNCTION__, __LINE__,                     \
## __VA_ARGS__)
// Debug log
#define VLOGD(fmt, ...) if (m_DebugLevelSets & LVL_D) \
  fprintf(stderr, LOG_TAG "%s DEBUG:%s::%d " fmt "\n",   \
      GetTimeStamp(), __FUNCTION__, __LINE__,                     \
## __VA_ARGS__)

// Verbose log
#define VLOGV(fmt, ...) if (m_DebugLevelSets & LVL_V) \
  fprintf(stderr, LOG_TAG "%s VERBOSE:%s::%d " fmt "\n",   \
      GetTimeStamp(), __FUNCTION__, __LINE__,                     \
## __VA_ARGS__)

// Function Enter/Exit
#define VLOGF(fmt, ...) if (m_DebugLevelSets & LVL_V) \
  fprintf(stderr, LOG_TAG "%s FUNC:%s::%d " fmt "\n",   \
      GetTimeStamp(), __FUNCTION__, __LINE__,                     \
## __VA_ARGS__)

#endif

#define FUNCTION_ENTER() VLOGF("+++++++++++++++++++Enter")
#define FUNCTION_EXIT() VLOGF("-------------------Exit")
