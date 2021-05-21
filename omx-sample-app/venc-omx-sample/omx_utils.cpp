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

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
//  #include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <errno.h>
#include <math.h>

#include <gralloc_priv.h>

/* def: StoreMetaDataInBuffersParams */
#include <media/hardware/HardwareAPI.h>

#include <vidc/media/msm_media_info.h>

//  OMX
#include "OMX_QCOMExtns.h"
#include "OMX_Core.h"
#include "OMX_Video.h"
#include "OMX_VideoExt.h"
#include "OMX_IndexExt.h"
#include "OMX_Component.h"
#include "OMX_Types.h"
#include "extra_data_handler.h"

// log control
extern uint32_t m_DebugLevelSets;

// fps control
extern int32_t m_ControlFps;

// test mode
extern int32_t m_TestMode;

// metadata mode
extern int32_t m_MetadataMode;

#include "video_debug.h"

#include "memory_manager.h"
#include "video_queue.h"
#include "omx_utils.h"

#define CHECK_HANDLE(handle) if (handle == NULL) \
{ \
  VLOGE("Invalid handle, Exit"); \
  FUNCTION_EXIT(); \
  return false; \
}

#define CHECK_RESULT(str, result) if ((result != OMX_ErrorNone) && (result != OMX_ErrorNoMore)) \
{ \
  VLOGE("%s result: %d", str, result); \
  FUNCTION_EXIT(); \
  return false; \
}

#define CHECK_BOOL(str, result) if (result == false) \
{ \
  VLOGE("%s result: %d", str, result); \
  FUNCTION_EXIT(); \
  return false; \
}

#define OMX_SPEC_VERSION  0x00000101
#define OMX_INIT_STRUCT(_s_, _name_)         \
  memset((_s_), 0x0, sizeof(_name_));      \
(_s_)->nSize = sizeof(_name_);               \
(_s_)->nVersion.nVersion = OMX_SPEC_VERSION

#define Log2(den, power)   { \
  OMX_U32 temp = den; \
  power = 0; \
  while((0 == (temp & 0x1)) && power < 16) { \
    temp >>=0x1; \
    power++; \
  } \
}

#define FractionToQ16(q, v) { (q) = (unsigned int) (65536*(double)(v)); }

#define OMX_INIT_STRUCT_SIZE(_s_, _name_)            \
  (_s_)->nSize = sizeof(_name_);               \
(_s_)->nVersion.nVersion = OMX_SPEC_VERSION

// A pointer to this struct is passed to OMX_SetParameter when the extension
// index for the 'OMX.google.android.index.prependSPSPPSToIDRFrames' extension
// (OMX_QcomIndexParamSequenceHeaderWithIDR)
// A successful result indicates that future IDR frames will be prefixed by SPS/PPS.
typedef struct PrependSPSPPSToIDRFramesParams {
  OMX_U32 nSize;
  OMX_VERSIONTYPE nVersion;
  OMX_BOOL bEnable;
} PrependSPSPPSToIDRFramesParams;

#define DEADVALUE ((OMX_S32) 0xDEADDEAD)  // in decimal : 3735936685

//////////////////////////
/* MPEG4 profile and level table*/
static const unsigned int mpeg4_profile_level_table[][5] = {
  /*max mb per frame, max mb per sec, max bitrate, level, profile*/
  {99, 1485, 64000, OMX_VIDEO_MPEG4Level0, OMX_VIDEO_MPEG4ProfileSimple},
  {99, 1485, 64000, OMX_VIDEO_MPEG4Level1, OMX_VIDEO_MPEG4ProfileSimple},
  {396, 5940, 128000, OMX_VIDEO_MPEG4Level2, OMX_VIDEO_MPEG4ProfileSimple},
  {396, 11880, 384000, OMX_VIDEO_MPEG4Level3, OMX_VIDEO_MPEG4ProfileSimple},
  {1200, 36000, 4000000, OMX_VIDEO_MPEG4Level4a, OMX_VIDEO_MPEG4ProfileSimple},
  {1620, 40500, 8000000, OMX_VIDEO_MPEG4Level5, OMX_VIDEO_MPEG4ProfileSimple},
  {3600, 108000, 12000000, OMX_VIDEO_MPEG4Level5, OMX_VIDEO_MPEG4ProfileSimple},
  {32400, 972000, 20000000, OMX_VIDEO_MPEG4Level5, OMX_VIDEO_MPEG4ProfileSimple},
  {34560, 1036800, 20000000, OMX_VIDEO_MPEG4Level5, OMX_VIDEO_MPEG4ProfileSimple},
  {0, 0, 0, 0, 0},

  {99, 1485, 128000, OMX_VIDEO_MPEG4Level0, OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {99, 1485, 128000, OMX_VIDEO_MPEG4Level1, OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {396, 5940, 384000, OMX_VIDEO_MPEG4Level2, OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {396, 11880, 768000, OMX_VIDEO_MPEG4Level3, OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {792, 23760, 3000000, OMX_VIDEO_MPEG4Level4, OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {1620, 48600, 8000000, OMX_VIDEO_MPEG4Level5, OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {32400, 972000, 20000000, OMX_VIDEO_MPEG4Level5, OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {34560, 1036800, 20000000, OMX_VIDEO_MPEG4Level5, OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {0, 0, 0, 0, 0},
};

/* H264 profile and level table*/
static const unsigned int h264_profile_level_table[][5] = {
  /*max mb per frame, max mb per sec, max bitrate, level, profile*/
  {99, 1485, 64000, OMX_VIDEO_AVCLevel1, OMX_VIDEO_AVCProfileBaseline},
  {99, 1485, 128000, OMX_VIDEO_AVCLevel1b, OMX_VIDEO_AVCProfileBaseline},
  {396, 3000, 192000, OMX_VIDEO_AVCLevel11, OMX_VIDEO_AVCProfileBaseline},
  {396, 6000, 384000, OMX_VIDEO_AVCLevel12, OMX_VIDEO_AVCProfileBaseline},
  {396, 11880, 768000, OMX_VIDEO_AVCLevel13, OMX_VIDEO_AVCProfileBaseline},
  {396, 11880, 2000000, OMX_VIDEO_AVCLevel2, OMX_VIDEO_AVCProfileBaseline},
  {792, 19800, 4000000, OMX_VIDEO_AVCLevel21, OMX_VIDEO_AVCProfileBaseline},
  {1620, 20250, 4000000, OMX_VIDEO_AVCLevel22, OMX_VIDEO_AVCProfileBaseline},
  {1620, 40500, 10000000, OMX_VIDEO_AVCLevel3, OMX_VIDEO_AVCProfileBaseline},
  {3600, 108000, 14000000, OMX_VIDEO_AVCLevel31, OMX_VIDEO_AVCProfileBaseline},
  {5120, 216000, 20000000, OMX_VIDEO_AVCLevel32, OMX_VIDEO_AVCProfileBaseline},
  {8192, 245760, 20000000, OMX_VIDEO_AVCLevel4, OMX_VIDEO_AVCProfileBaseline},
  {8192, 245760, 50000000, OMX_VIDEO_AVCLevel41, OMX_VIDEO_AVCProfileBaseline},
  {8704, 522240, 50000000, OMX_VIDEO_AVCLevel42, OMX_VIDEO_AVCProfileBaseline},
  {22080, 589824, 135000000, OMX_VIDEO_AVCLevel5, OMX_VIDEO_AVCProfileBaseline},
  {36864, 983040, 240000000, OMX_VIDEO_AVCLevel51, OMX_VIDEO_AVCProfileBaseline},
  {36864, 2073600, 240000000, OMX_VIDEO_AVCLevel52, OMX_VIDEO_AVCProfileBaseline},
  {139264, 4177920, 240000000, OMX_VIDEO_AVCLevel6, OMX_VIDEO_AVCProfileBaseline},
  {139264, 8355840, 480000000, OMX_VIDEO_AVCLevel61, OMX_VIDEO_AVCProfileBaseline},
  {139264, 16711680, 800000000, OMX_VIDEO_AVCLevel62, OMX_VIDEO_AVCProfileBaseline},
  {0, 0, 0, 0, 0},

  {99, 1485, 64000, OMX_VIDEO_AVCLevel1, OMX_VIDEO_AVCProfileMain},
  {99, 1485, 128000, OMX_VIDEO_AVCLevel1b, OMX_VIDEO_AVCProfileMain},
  {396, 3000, 192000, OMX_VIDEO_AVCLevel11, OMX_VIDEO_AVCProfileMain},
  {396, 6000, 384000, OMX_VIDEO_AVCLevel12, OMX_VIDEO_AVCProfileMain},
  {396, 11880, 768000, OMX_VIDEO_AVCLevel13, OMX_VIDEO_AVCProfileMain},
  {396, 11880, 2000000, OMX_VIDEO_AVCLevel2, OMX_VIDEO_AVCProfileMain},
  {792, 19800, 4000000, OMX_VIDEO_AVCLevel21, OMX_VIDEO_AVCProfileMain},
  {1620, 20250, 4000000, OMX_VIDEO_AVCLevel22, OMX_VIDEO_AVCProfileMain},
  {1620, 40500, 10000000, OMX_VIDEO_AVCLevel3, OMX_VIDEO_AVCProfileMain},
  {3600, 108000, 14000000, OMX_VIDEO_AVCLevel31, OMX_VIDEO_AVCProfileMain},
  {5120, 216000, 20000000, OMX_VIDEO_AVCLevel32, OMX_VIDEO_AVCProfileMain},
  {8192, 245760, 20000000, OMX_VIDEO_AVCLevel4, OMX_VIDEO_AVCProfileMain},
  {8192, 245760, 50000000, OMX_VIDEO_AVCLevel41, OMX_VIDEO_AVCProfileMain},
  {8704, 522240, 50000000, OMX_VIDEO_AVCLevel42, OMX_VIDEO_AVCProfileMain},
  {22080, 589824, 135000000, OMX_VIDEO_AVCLevel5, OMX_VIDEO_AVCProfileMain},
  {36864, 983040, 240000000, OMX_VIDEO_AVCLevel51, OMX_VIDEO_AVCProfileMain},
  {36864, 2073600, 240000000, OMX_VIDEO_AVCLevel52, OMX_VIDEO_AVCProfileMain},
  {139264, 4177920, 240000000, OMX_VIDEO_AVCLevel6, OMX_VIDEO_AVCProfileMain},
  {139264, 8355840, 480000000, OMX_VIDEO_AVCLevel61, OMX_VIDEO_AVCProfileMain},
  {139264, 16711680, 800000000, OMX_VIDEO_AVCLevel62, OMX_VIDEO_AVCProfileMain},
  {0, 0, 0, 0, 0}
};

/* H263 profile and level table*/
static const unsigned int h263_profile_level_table[][5]= {
  /*max mb per frame, max mb per sec, max bitrate, level, profile*/
  {99, 1485, 64000, OMX_VIDEO_H263Level10, OMX_VIDEO_H263ProfileBaseline},
  {396, 5940, 128000, OMX_VIDEO_H263Level20, OMX_VIDEO_H263ProfileBaseline},
  {396, 11880, 384000, OMX_VIDEO_H263Level30, OMX_VIDEO_H263ProfileBaseline},
  {396, 11880, 2048000, OMX_VIDEO_H263Level40, OMX_VIDEO_H263ProfileBaseline},
  {99, 1485, 128000, OMX_VIDEO_H263Level45, OMX_VIDEO_H263ProfileBaseline},
  {396, 19800, 4096000, OMX_VIDEO_H263Level50, OMX_VIDEO_H263ProfileBaseline},
  {810, 40500, 8192000, OMX_VIDEO_H263Level60, OMX_VIDEO_H263ProfileBaseline},
  {1620, 81000, 16384000, OMX_VIDEO_H263Level70, OMX_VIDEO_H263ProfileBaseline},
  {32400, 972000, 20000000, OMX_VIDEO_H263Level60, OMX_VIDEO_H263ProfileBaseline},
  {34560, 1036800, 20000000, OMX_VIDEO_H263Level70, OMX_VIDEO_H263ProfileBaseline},
  {0, 0, 0, 0, 0}
};

static const unsigned int VP8_profile_level_table[][5] = {
  /*max mb per frame, max mb per sec, max bitrate, level, profile*/
  /*{99, 1485, 64000, OMX_VIDEO_H263Level10, OMX_VIDEO_H263ProfileBaseline},
  {396, 5940, 128000, OMX_VIDEO_H263Level20, OMX_VIDEO_H263ProfileBaseline},
  {396, 11880, 384000, OMX_VIDEO_H263Level30, OMX_VIDEO_H263ProfileBaseline},
  {396, 11880, 2048000, OMX_VIDEO_H263Level40, OMX_VIDEO_H263ProfileBaseline},
  {99, 1485, 128000, OMX_VIDEO_H263Level45, OMX_VIDEO_H263ProfileBaseline},
  {396, 19800, 4096000, OMX_VIDEO_H263Level50, OMX_VIDEO_H263ProfileBaseline},
  {810, 40500, 8192000, OMX_VIDEO_H263Level60, OMX_VIDEO_H263ProfileBaseline},
  {1620, 81000, 16384000, OMX_VIDEO_H263Level70, OMX_VIDEO_H263ProfileBaseline},
  {32400, 972000, 20000000, OMX_VIDEO_H263Level70, OMX_VIDEO_H263ProfileBaseline},
  {34560, 1036800, 20000000, OMX_VIDEO_H263Level70, OMX_VIDEO_H263ProfileBaseline},*/
  {36864, 1105920, 40000000, OMX_VIDEO_VP8Level_Version0, OMX_VIDEO_VP8ProfileMain},
  {0, 0, 0, 0, 0}
};

/* HEVC profile and level table*/
static const unsigned int hevc_profile_level_table[][5] = {
  /*max mb per frame, max mb per sec, max bitrate, level, profile*/
  {144, 2160, 128000, OMX_VIDEO_HEVCMainTierLevel1, OMX_VIDEO_HEVCProfileMain},
  {480, 1440, 1500000, OMX_VIDEO_HEVCMainTierLevel2, OMX_VIDEO_HEVCProfileMain},
  {960, 28800, 3000000, OMX_VIDEO_HEVCMainTierLevel21, OMX_VIDEO_HEVCProfileMain},
  {2160, 64800, 6000000, OMX_VIDEO_HEVCMainTierLevel3, OMX_VIDEO_HEVCProfileMain},
  {3840, 129600, 10000000, OMX_VIDEO_HEVCMainTierLevel31, OMX_VIDEO_HEVCProfileMain},
  {8704, 261120, 12000000, OMX_VIDEO_HEVCMainTierLevel4, OMX_VIDEO_HEVCProfileMain},
  {8704, 522240, 20000000, OMX_VIDEO_HEVCMainTierLevel41, OMX_VIDEO_HEVCProfileMain},
  {34816, 1044480, 25000000, OMX_VIDEO_HEVCMainTierLevel5, OMX_VIDEO_HEVCProfileMain},
  {34816, 2088960, 40000000, OMX_VIDEO_HEVCMainTierLevel51, OMX_VIDEO_HEVCProfileMain},
  {8704, 522240, 50000000, OMX_VIDEO_HEVCHighTierLevel41, OMX_VIDEO_HEVCProfileMain},
  {34816, 2088960, 160000000, OMX_VIDEO_HEVCHighTierLevel51, OMX_VIDEO_HEVCProfileMain},
  {34816, 4177920, 240000000, OMX_VIDEO_HEVCHighTierLevel52, OMX_VIDEO_HEVCProfileMain},
  {139264, 8355840, 240000000, OMX_VIDEO_HEVCHighTierLevel6, OMX_VIDEO_HEVCProfileMain},
  {139264, 4147200, 480000000, OMX_VIDEO_HEVCHighTierLevel61, OMX_VIDEO_HEVCProfileMain},
  {139264, 16711680, 800000000, OMX_VIDEO_HEVCHighTierLevel62, OMX_VIDEO_HEVCProfileMain},
  {0, 0, 0, 0, 0},
  {144, 2160, 128000, OMX_VIDEO_HEVCMainTierLevel1, OMX_VIDEO_HEVCProfileMain10},
  {480, 1440, 1500000, OMX_VIDEO_HEVCMainTierLevel2, OMX_VIDEO_HEVCProfileMain10},
  {960, 28800, 3000000, OMX_VIDEO_HEVCMainTierLevel21, OMX_VIDEO_HEVCProfileMain10},
  {2160, 64800, 6000000, OMX_VIDEO_HEVCMainTierLevel3, OMX_VIDEO_HEVCProfileMain10},
  {3840, 129600, 10000000, OMX_VIDEO_HEVCMainTierLevel31, OMX_VIDEO_HEVCProfileMain10},
  {8704, 261120, 12000000, OMX_VIDEO_HEVCMainTierLevel4, OMX_VIDEO_HEVCProfileMain10},
  {8704, 522240, 20000000, OMX_VIDEO_HEVCMainTierLevel41, OMX_VIDEO_HEVCProfileMain10},
  {34816, 1044480, 25000000, OMX_VIDEO_HEVCMainTierLevel5, OMX_VIDEO_HEVCProfileMain10},
  {34816, 2088960, 40000000, OMX_VIDEO_HEVCMainTierLevel51, OMX_VIDEO_HEVCProfileMain10},
  {8704, 522240, 50000000, OMX_VIDEO_HEVCHighTierLevel41, OMX_VIDEO_HEVCProfileMain10},
  {34816, 2088960, 160000000, OMX_VIDEO_HEVCHighTierLevel51, OMX_VIDEO_HEVCProfileMain10},
  {34816, 4177920, 240000000, OMX_VIDEO_HEVCHighTierLevel52, OMX_VIDEO_HEVCProfileMain10},
  {139264, 8355840, 240000000, OMX_VIDEO_HEVCHighTierLevel6, OMX_VIDEO_HEVCProfileMain10},
  {139264, 4147200, 480000000, OMX_VIDEO_HEVCHighTierLevel61, OMX_VIDEO_HEVCProfileMain10},
  {139264, 16711680, 800000000, OMX_VIDEO_HEVCHighTierLevel62, OMX_VIDEO_HEVCProfileMain10},
  {0, 0, 0, 0, 0},
};

#define MAX_INPUT_BUFFER_NUM 20
#define MAX_OUTPUT_BUFFER_NUM 20

#define DEFAULT_TILE_DIMENSION 512
#define DEFAULT_TILE_COUNT 40

#define FRAME_PACK_SIZE 18

static OMX_STATETYPE m_State = OMX_StateLoaded;
static OMX_STATETYPE m_PendingEstate = OMX_StateLoaded;
static OMX_HANDLETYPE m_Handle = NULL;

static int32_t m_InputBufferSize = 0;
static int32_t m_InputBufferCount = 0;
static int32_t m_OutputBufferSize = 0;
static int32_t m_OutputBufferCount = 0;
static int32_t m_MetaFrameBufferSize = 0;

static unsigned char * m_AdvancedInputBuffer = NULL;  // total size: size * count
static int32_t m_ReadFrameLen = 0;

static const int OMX_BUFFERS_NUM = 20;
OMX_BUFFERHEADERTYPE* m_InputBufferHeaders[OMX_BUFFERS_NUM] = {NULL};
OMX_BUFFERHEADERTYPE* m_OutputBufferHeaders[OMX_BUFFERS_NUM] = {NULL};

int32_t m_ReadFramNum = 0;
long long m_FrameTime = 0;
long long m_TimeStamp = 0;
int64_t m_EncodeTotalTimeActal = 0;
int32_t m_CurrentEncodeFrameNum = 0;
int64_t m_EncodeFrameTimeMax = 0;
int64_t m_EncodeFrameTimeMin = 0;
timeval m_EncodeFrameStartTime[OMX_BUFFERS_NUM * 2] = {};  // used to compute encode frame performance
timeval m_EncodeFrameEndTime[OMX_BUFFERS_NUM * 2] = {};  // used to compute encode frame performance

struct EncodeIon* m_IonDataArray = NULL;

enum MetaBufferType {
  CameraSource = 0,
  GrallocSource = 1  // kMetadataBufferTypeGrallocSource
};

struct NativeHandle {
  int version;        /* sizeof(native_handle_t) */
  int numFds;         /* number of file-descriptors at &data[0] */
  int numInts;        /* number of ints at &data[numFds] */
  unsigned long data[0];        /* numFds + numInts ints */
};
// typedef NativeHandle* BufferHandle;
typedef private_handle_t* BufferHandle;
struct MetaBuffer {
  MetaBufferType buffer_type;
  BufferHandle meta_handle;
};

using android::StoreMetaDataInBuffersParams;

/**************************** Omx utils API define **************************/
OMX_ERRORTYPE GetVendorExtension(char* ext_name,
    OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *ext_type) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  FUNCTION_ENTER();

  if (ext_type == NULL || ext_name == NULL) {
    VLOGE("parameter is NULL");
    FUNCTION_EXIT();
    return OMX_ErrorUndefined;
  }

  // check all supported vendor extensions and get the index for the expected one
  for (OMX_U32 index = 0; ; index++) {
    ext_type->nIndex = index;
    result = OMX_GetConfig(m_Handle,
        (OMX_INDEXTYPE)OMX_IndexConfigAndroidVendorExtension,
        (OMX_PTR)ext_type);

    if (result == OMX_ErrorNone) {
      if (!strcmp(reinterpret_cast<char*>(ext_type->cName), ext_name)) {
        VLOGD("find extension %s\n", ext_name);
        FUNCTION_EXIT();
        return result;
      }
    } else {
      VLOGE("\nfailed to get vendor extension %s", ext_name);
      FUNCTION_EXIT();
      return OMX_ErrorUndefined;
    }
  }

  FUNCTION_EXIT();
  return result;
}

OMX_ERRORTYPE SetVendorRateControlMode(OMX_VIDEO_CONTROLRATETYPE control_rate) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE* ext_type = NULL;
  char extension_name[] = "qti-ext-enc-bitrate-mode";
  OMX_U32 param_size_used = 1;   // TODO: check this value
  OMX_U32 size = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE) +
    (param_size_used - 1) * sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE::nParam);

  FUNCTION_ENTER();

  ext_type = reinterpret_cast<OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE*>(malloc(size));
  if (!ext_type) {
    VLOGE("alloc memory failed");
    FUNCTION_EXIT();
    return OMX_ErrorUndefined;
  }

  memset(ext_type, 0, sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE));
  ext_type->nSize = size;
  ext_type->nParamSizeUsed = param_size_used;
  result = GetVendorExtension(extension_name, ext_type);

  if (result == OMX_ErrorNone) {
    VLOGD("setting eControlRate: %d", control_rate);
    ext_type->nParam[0].bSet = OMX_TRUE;
    ext_type->nParam[0].nInt32 = control_rate;
    result = OMX_SetConfig(m_Handle,
        (OMX_INDEXTYPE)OMX_IndexConfigAndroidVendorExtension,
        (OMX_PTR)ext_type);
  }

  if (result != OMX_ErrorNone) {
    VLOGE("failed to set eControlRate, error:%d", result);
  }

  if (ext_type) {
    free(ext_type);
    ext_type = NULL;
  }

  FUNCTION_EXIT();
  return result;
}

void DumpFramePackArrangement(OMX_QCOM_FRAME_PACK_ARRANGEMENT frame_packing_arrangement) {
  VLOGD("id (0x%x)",
      frame_packing_arrangement.id);
  VLOGD("cancel_flag (0x%x)",
      frame_packing_arrangement.cancel_flag);
  VLOGD("type (0x%x)",
      frame_packing_arrangement.type);
  VLOGD("quincunx_sampling_flag (0x%x)",
      frame_packing_arrangement.quincunx_sampling_flag);
  VLOGD("content_interpretation_type (0x%x)",
      frame_packing_arrangement.content_interpretation_type);
  VLOGD("spatial_flipping_flag (0x%x)",
      frame_packing_arrangement.spatial_flipping_flag);
  VLOGD("frame0_flipped_flag (0x%x)",
      frame_packing_arrangement.frame0_flipped_flag);
  VLOGD("field_views_flag (0x%x)",
      frame_packing_arrangement.field_views_flag);
  VLOGD("current_frame_is_frame0_flag (0x%x)",
      frame_packing_arrangement.current_frame_is_frame0_flag);
  VLOGD("frame0_self_contained_flag (0x%x)",
      frame_packing_arrangement.frame0_self_contained_flag);
  VLOGD("frame1_self_contained_flag (0x%x)",
      frame_packing_arrangement.frame1_self_contained_flag);
  VLOGD("frame0_grid_position_x (%d)",
      frame_packing_arrangement.frame0_grid_position_x);
  VLOGD("frame0_grid_position_y (%d)",
      frame_packing_arrangement.frame0_grid_position_y);
  VLOGD("frame1_grid_position_x (%d)",
      frame_packing_arrangement.frame1_grid_position_x);
  VLOGD("frame1_grid_position_y (%d)",
      frame_packing_arrangement.frame1_grid_position_y);
  VLOGD("reserved_byte (%d)",
      frame_packing_arrangement.reserved_byte);
  VLOGD("repetition_period (%d)",
      frame_packing_arrangement.repetition_period);
  VLOGD("extension_flag (0x%x)",
      frame_packing_arrangement.extension_flag);
}

bool SetMetaDataMode(OMX_U32 portindex) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_PARAM_PORTDEFINITIONTYPE portdef;

  StoreMetaDataInBuffersParams metadata_mode;
  OMX_INIT_STRUCT(&metadata_mode, StoreMetaDataInBuffersParams);
  metadata_mode.nPortIndex = portindex;
  metadata_mode.bStoreMetaData = OMX_TRUE;
  result = OMX_SetParameter(m_Handle,
      (OMX_INDEXTYPE)OMX_QcomIndexParamVideoMetaBufferMode,
      (OMX_PTR)&metadata_mode);
  CHECK_RESULT("Error OMX_QcomIndexParamVideoEncodeMetaBufferMode:", result);

  OMX_INIT_STRUCT(&portdef, OMX_PARAM_PORTDEFINITIONTYPE);
  result = OMX_GetParameter(m_Handle, OMX_IndexParamPortDefinition, &portdef);

  CHECK_RESULT("error get OMX_IndexParamPortDefinition:", result);
  return true;
}

bool SetInPortParameters(OMX_U32 width, OMX_U32 height,
    OMX_U32 format, OMX_U32 frame_rate) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_PARAM_PORTDEFINITIONTYPE portdef;  // OMX_IndexParamPortDefinition

#ifdef QTI_EXT
  OMX_QCOM_PARAM_PORTDEFINITIONTYPE port_def_type;
#endif

  FUNCTION_ENTER();

  OMX_INIT_STRUCT(&portdef, OMX_PARAM_PORTDEFINITIONTYPE);
  portdef.nPortIndex = (OMX_U32) PORT_INDEX_IN;  // input
  result = OMX_GetParameter(m_Handle,
      OMX_IndexParamPortDefinition,
      &portdef);
  CHECK_RESULT("get in port default parameter:", result);

  portdef.format.video.nFrameWidth = width;
  portdef.format.video.nFrameHeight = height;
  portdef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)format;
  FractionToQ16(portdef.format.video.xFramerate, frame_rate);

  VLOGD("set in port param: width:%d, height:%d, format:0x%x, frameRate:%d",
      width, height,
      portdef.format.video.eColorFormat,
      portdef.format.video.xFramerate);

  result = OMX_SetParameter(m_Handle,
      OMX_IndexParamPortDefinition,
      &portdef);
  CHECK_RESULT("set in port parameter:", result);

  if (m_MetadataMode) {
    SetMetaDataMode(portdef.nPortIndex);
  }

  OMX_INIT_STRUCT(&portdef, OMX_PARAM_PORTDEFINITIONTYPE);
  portdef.nPortIndex = (OMX_U32) PORT_INDEX_IN;  // input
  result = OMX_GetParameter(m_Handle,
      OMX_IndexParamPortDefinition,
      &portdef);
  CHECK_RESULT("get in port parameter:", result);
  m_InputBufferSize = portdef.nBufferSize;
  m_InputBufferCount = portdef.nBufferCountActual;

#ifdef QTI_EXT
  OMX_QCOM_PARAM_PORTDEFINITIONTYPE port_def_type;

  OMX_INIT_STRUCT(&qPortDefnType, OMX_QCOM_PARAM_PORTDEFINITIONTYPE);
  qPortDefnType.nPortIndex = PORT_INDEX_IN;
  qPortDefnType.nMemRegion = OMX_QCOM_MemRegionEBI1;
  qPortDefnType.nSize = sizeof(OMX_QCOM_PARAM_PORTDEFINITIONTYPE);
  result = OMX_SetParameter(m_Handle,
      (OMX_INDEXTYPE)OMX_QcomIndexPortDefn,
      &port_def_type);
  CHECK_RESULT("set in port QTI_EXT:", result);
#endif

  FUNCTION_EXIT();
  return true;
}

bool SetOutPortParameters(OMX_U32 width, OMX_U32 height,
    OMX_U32 framerate, OMX_U32 bitrate, OMX_U32 codec) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_PARAM_PORTDEFINITIONTYPE portdef;

  FUNCTION_ENTER();

  OMX_INIT_STRUCT(&portdef, OMX_PARAM_PORTDEFINITIONTYPE);
  portdef.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  result = OMX_GetParameter(m_Handle,
      OMX_IndexParamPortDefinition,
      &portdef);
  CHECK_RESULT("get out port default parameter", result);

  portdef.format.video.nFrameWidth = width;
  portdef.format.video.nFrameHeight = height;
  portdef.format.video.nBitrate = bitrate;
  FractionToQ16(portdef.format.video.xFramerate, framerate);

  if (codec == OMX_VIDEO_CodingMPEG4) {
    portdef.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  }

  VLOGD("width:%d, height:%d, bitrate:%d, nFramerate:%d, xFramerate:%d",
      width, height, bitrate, framerate,
      portdef.format.video.xFramerate);

  result = OMX_SetParameter(m_Handle,
      OMX_IndexParamPortDefinition,
      &portdef);
  CHECK_RESULT("set out port parameter:", result);

  OMX_INIT_STRUCT(&portdef, OMX_PARAM_PORTDEFINITIONTYPE);
  portdef.nPortIndex = (OMX_U32) PORT_INDEX_OUT;  // input
  result = OMX_GetParameter(m_Handle,
      OMX_IndexParamPortDefinition,
      &portdef);
  CHECK_RESULT("get out port parameter:", result);
  m_OutputBufferSize = portdef.nBufferSize;
  m_OutputBufferCount = portdef.nBufferCountActual;

  VLOGD("output buffer size: %d, count: %d",
      m_OutputBufferSize,
      m_OutputBufferCount);

  FUNCTION_EXIT();
  return true;
}

bool GetDefaultUserProfile(OMX_VIDEO_CODINGTYPE codec,
    OMX_U32 width, OMX_U32 height,
    OMX_U32 framerate, OMX_U32 bitrate,
    OMX_U32 *p_user_profile, OMX_U32 *p_elevel) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  unsigned const int *profile_tbl = NULL;
  OMX_U32 mb_per_sec, mb_per_frame;
  bool profile_level_found = false;
  OMX_U32 e_profile, e_level;

  FUNCTION_ENTER();

  if (p_user_profile == NULL || p_elevel == NULL) {
    VLOGE("bad parameter: %p, %p", p_user_profile, p_elevel);
    return false;
  }

  VLOGD("eCodec:%d, w:%d, h:%d, frameRate:%d, bitRate:%d",
      codec, width, height, framerate, bitrate);

  // validate the ht,width,fps,bitrate and set the appropriate profile and level
  if  (codec == OMX_VIDEO_CodingMPEG4) {
    profile_tbl = (unsigned int const *)mpeg4_profile_level_table;
  } else if (codec == OMX_VIDEO_CodingAVC) {
    profile_tbl = (unsigned int const *)h264_profile_level_table;
  } else if (codec == OMX_VIDEO_CodingH263) {
    profile_tbl = (unsigned int const *)h263_profile_level_table;
  } else if (codec == OMX_VIDEO_CodingVP8) {
    profile_tbl = (unsigned int const *)VP8_profile_level_table;
  } else if (codec == OMX_VIDEO_CodingHEVC) {
    profile_tbl = (unsigned int const *)hevc_profile_level_table;
  } else if (codec == OMX_VIDEO_CodingImageHEIC) {
    *p_user_profile = OMX_VIDEO_HEVCProfileMainStill;
    *p_elevel = OMX_VIDEO_HEVCHighTierLevel6;
    profile_level_found = true;
    FUNCTION_EXIT();
    return true;
  }

  if (profile_tbl == NULL) {
    VLOGE("Unsupported  eCodec:%d", codec);
    FUNCTION_EXIT();
    return false;
  }
  mb_per_frame = ((height + 15) >> 4) * ((width + 15) >> 4);
  mb_per_sec = mb_per_frame*(framerate);

  VLOGD("mb_per_frame: %d, mb_per_sec: %d, nBitrate: %d",
      mb_per_frame, mb_per_sec, bitrate);

  do {
    if (mb_per_frame <= (unsigned int)profile_tbl[0]) {
      if (mb_per_sec <= (unsigned int)profile_tbl[1]) {
        if (bitrate <= (unsigned int)profile_tbl[2]) {
          *p_elevel = static_cast<int>(profile_tbl[3]);
          *p_user_profile = static_cast<int>(profile_tbl[4]);
          VLOGD("find profile:%d level: %d",
              *p_user_profile, *p_elevel);
          profile_level_found = true;
          break;
        }
      }
    }
    profile_tbl = profile_tbl + 5;
  } while (profile_tbl[0] != 0);

  if (profile_level_found != true) {
    VLOGE("Unsupported w:%d, h:%d, frameRate:%d, bitRate:%d, eCodec:%d",
        width, height, framerate, bitrate, codec);
  }

  FUNCTION_EXIT();
  return profile_level_found == true ? true : false;
}

bool ConfigureH263Codec(OMX_U32 user_profile, OMX_U32 e_level,
    OMX_U32 framerate) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_VIDEO_PARAM_H263TYPE h263;

  FUNCTION_ENTER();

  VLOGD("userProfile:%d, eLevel:%d, frameRate:%d",
      user_profile, e_level, framerate);

  OMX_INIT_STRUCT(&h263, OMX_VIDEO_PARAM_H263TYPE);
  result = OMX_GetParameter(m_Handle,
      OMX_IndexParamVideoH263,
      &h263);
  CHECK_RESULT("get h263 default parameter", result);

  h263.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  h263.nPFrames = framerate * 2 - 1;  // intra period
  h263.nBFrames = 0;
  h263.eProfile = (OMX_VIDEO_H263PROFILETYPE)user_profile;
  h263.eLevel = (OMX_VIDEO_H263LEVELTYPE)e_level;
  h263.bPLUSPTYPEAllowed = OMX_FALSE;
  h263.nAllowedPictureTypes = 2;
  h263.bForceRoundingTypeToZero = OMX_TRUE;
  h263.nPictureHeaderRepetition = 0;
  h263.nGOBHeaderInterval = 1;
  result = OMX_SetParameter(m_Handle,
      OMX_IndexParamVideoH263,
      &h263);
  CHECK_RESULT("set h263 parameter:", result);

  FUNCTION_EXIT();
  return true;
}

bool ConfigureMPEG4Codec(OMX_U32 user_profile, OMX_U32 e_level,
    OMX_U32 framerate) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE profile_level;

  FUNCTION_ENTER();

  VLOGD("userProfile:%d, eLevel:%d, frameRate:%d",
      user_profile, e_level, framerate);

  OMX_INIT_STRUCT(&profile_level, OMX_VIDEO_PARAM_PROFILELEVELTYPE);
  profile_level.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  profile_level.eProfile = user_profile;
  profile_level.eLevel =  e_level;
  result = OMX_SetParameter(m_Handle,
      OMX_IndexParamVideoProfileLevelCurrent,
      &profile_level);
  CHECK_RESULT("set MPEG4 profile:", result);

  // check set parameter result:
  OMX_INIT_STRUCT(&profile_level, OMX_VIDEO_PARAM_PROFILELEVELTYPE);
  result = OMX_GetParameter(m_Handle,
      OMX_IndexParamVideoProfileLevelCurrent,
      &profile_level);
  CHECK_RESULT("get MPEG4 set profile result:", result);
  VLOGD("get MPEG4 real Profile = %d level = %d",
      profile_level.eProfile, profile_level.eLevel);

  OMX_VIDEO_PARAM_MPEG4TYPE mp4;  // OMX_IndexParamVideoMpeg4
  OMX_INIT_STRUCT(&mp4, OMX_VIDEO_PARAM_MPEG4TYPE);
  result = OMX_GetParameter(m_Handle,
      OMX_IndexParamVideoMpeg4,
      &mp4);
  CHECK_RESULT("get MPEG4 default parameter:", result);

  mp4.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  mp4.nBFrames = 0;
  mp4.nIDCVLCThreshold = 0;
  mp4.bACPred = OMX_TRUE;
  mp4.nMaxPacketSize = 256;
  mp4.nTimeIncRes = 1000;
  mp4.nHeaderExtension = 0;
  mp4.bReversibleVLC = OMX_FALSE;
  result = OMX_SetParameter(m_Handle,
      OMX_IndexParamVideoMpeg4,
      &mp4);
  CHECK_RESULT("set MPEG4 parameter:", result);

  FUNCTION_EXIT();
  return true;
}

#define AVC_DEFAULT_P_FRAME_NUM 6
#define AVC_DEFAULT_B_FRAME_NUM 0
#define AVC_DEFAULT_REF_FRAME_NUM 1

bool ConfigureAvcCodec(int32_t user_profile, int32_t e_level,
    OMX_U32 framerate) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_VIDEO_PARAM_AVCTYPE avcdata;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE profile_level;

  FUNCTION_ENTER();

  VLOGD("userProfile:%d, eLevel:%d, frameRate:%d",
      user_profile, e_level, framerate);


  OMX_INIT_STRUCT(&profile_level, OMX_VIDEO_PARAM_PROFILELEVELTYPE);
  profile_level.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  profile_level.eProfile = user_profile;
  profile_level.eLevel =  e_level;
  result = OMX_SetParameter(m_Handle,
      OMX_IndexParamVideoProfileLevelCurrent,
      &profile_level);
  CHECK_RESULT("set avc profile:", result);

  // check set parameter result:
  OMX_INIT_STRUCT(&profile_level, OMX_VIDEO_PARAM_PROFILELEVELTYPE);
  result = OMX_GetParameter(m_Handle,
      OMX_IndexParamVideoProfileLevelCurrent,
      &profile_level);
  CHECK_RESULT("get avc set profile result:", result);
  VLOGD("get avc real Profile = %d level = %d",
      profile_level.eProfile, profile_level.eLevel);


  OMX_INIT_STRUCT(&avcdata, OMX_VIDEO_PARAM_AVCTYPE);
  avcdata.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
  result = OMX_GetParameter(m_Handle,
      OMX_IndexParamVideoAvc,
      &avcdata);
  CHECK_RESULT("get avc default parameter:", result);

  avcdata.nPFrames = AVC_DEFAULT_P_FRAME_NUM;  // update by user
  avcdata.nBFrames = AVC_DEFAULT_B_FRAME_NUM;  // update by user
  avcdata.bUseHadamard = OMX_FALSE;
  avcdata.nRefFrames = AVC_DEFAULT_REF_FRAME_NUM;
  avcdata.nRefIdx10ActiveMinus1 = 1;
  avcdata.nRefIdx11ActiveMinus1 = 0;
  avcdata.bEnableUEP = OMX_FALSE;
  avcdata.bEnableFMO = OMX_FALSE;
  avcdata.bEnableASO = OMX_FALSE;
  avcdata.bEnableRS = OMX_FALSE;
  avcdata.nAllowedPictureTypes = 2;
  avcdata.bFrameMBsOnly = OMX_FALSE;
  avcdata.bMBAFF = OMX_FALSE;
  avcdata.bWeightedPPrediction = OMX_FALSE;
  avcdata.nWeightedBipredicitonMode = 0;
  avcdata.bconstIpred = OMX_FALSE;
  avcdata.bDirect8x8Inference = OMX_FALSE;
  avcdata.bDirectSpatialTemporal = OMX_FALSE;
  avcdata.nPFrames = framerate * 2 - 1;
  avcdata.nBFrames = 0;

  avcdata.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterDisable;

  result = OMX_SetParameter(m_Handle,
      OMX_IndexParamVideoAvc,
      &avcdata);
  CHECK_RESULT("set avc parameter:", result);

  FUNCTION_EXIT();
  return true;
}

bool ConfigureHevcCodec(OMX_U32 user_profile, OMX_U32 e_level,
    OMX_U32 framerate) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_VIDEO_PARAM_HEVCTYPE hevcdata;

  FUNCTION_ENTER();
  VLOGD("userProfile:%d, eLevel:%d, frameRate:%d",
      user_profile, e_level, framerate);

  OMX_INIT_STRUCT(&hevcdata, OMX_VIDEO_PARAM_HEVCTYPE);
  result = OMX_GetParameter(m_Handle,
      (OMX_INDEXTYPE)OMX_IndexParamVideoHevc,
      &hevcdata);
  CHECK_RESULT("get HEVC default parameter:", result);

  hevcdata.eProfile = (OMX_VIDEO_HEVCPROFILETYPE)user_profile;
  hevcdata.eLevel = (OMX_VIDEO_HEVCLEVELTYPE)e_level;
  hevcdata.nKeyFrameInterval = framerate * 2 - 1;
  result = OMX_SetParameter(m_Handle,
      (OMX_INDEXTYPE)OMX_IndexParamVideoHevc,
      &hevcdata);
  CHECK_RESULT("set h265 parameter:", result);

  FUNCTION_EXIT();
  return true;
}

bool ConfigureVp8Codec(OMX_U32 user_profile, OMX_U32 e_level,
    OMX_U32 framerate) {
  OMX_ERRORTYPE result = OMX_ErrorNone;

  FUNCTION_ENTER();
  VLOGD("userProfile:%d, eLevel:%d, frameRate:%d",
      user_profile, e_level, framerate);

  OMX_VIDEO_PARAM_PROFILELEVELTYPE profile_level;
  OMX_INIT_STRUCT(&profile_level, OMX_VIDEO_PARAM_PROFILELEVELTYPE);
  profile_level.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  profile_level.eProfile = user_profile;
  profile_level.eLevel = e_level;
  result = OMX_SetParameter(m_Handle,
      OMX_IndexParamVideoProfileLevelCurrent,
      &profile_level);
  CHECK_RESULT("set Vp8 profile:", result);
  result = OMX_GetParameter(m_Handle,
      OMX_IndexParamVideoProfileLevelCurrent,
      &profile_level);
  VLOGD("Profile = %d level = %d", profile_level.eProfile, profile_level.eLevel);

  /*OMX_VIDEO_PARAM_VP8TYPE vp8data;
  OMX_INIT_STRUCT(&vp8data, OMX_VIDEO_PARAM_VP8TYPE);

  result = OMX_GetParameter(m_Handle,
      (OMX_INDEXTYPE)OMX_IndexParamVideoVp8,
      &vp8data);
  VLOGD("vp8 Profile = %d level = %d", vp8data.eProfile, vp8data.eLevel);
  CHECK_RESULT("get VP8 default parameter:", result);

  vp8data.eProfile = (OMX_VIDEO_VP8PROFILETYPE)user_profile;
  vp8data.eLevel = (OMX_VIDEO_VP8LEVELTYPE)e_level;
  vp8data.bErrorResilientMode = (OMX_BOOL)0;
  result = OMX_SetParameter(m_Handle,
      (OMX_INDEXTYPE)OMX_IndexParamVideoVp8,
      &vp8data);
  CHECK_RESULT("set vp8 parameter:", result);

  result = OMX_GetParameter(m_Handle,
      (OMX_INDEXTYPE)OMX_IndexParamVideoVp8,
      &vp8data);
  VLOGD("vp8 new Profile = %d level = %d", vp8data.eProfile, vp8data.eLevel);*/
  FUNCTION_EXIT();
  return true;
}

bool ConfigureHEICImageCodec(OMX_U32 user_profile, OMX_U32 e_level,
    OMX_U32 framerate) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE profile_level;

  FUNCTION_ENTER();
  VLOGD("userProfile:%d, eLevel:%d, frameRate:%d",
      user_profile, e_level, framerate);

  OMX_INIT_STRUCT(&profile_level, OMX_VIDEO_PARAM_PROFILELEVELTYPE);
  profile_level.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  profile_level.eProfile = user_profile;
  profile_level.eLevel =  e_level;
  result = OMX_SetParameter(m_Handle,
      OMX_IndexParamVideoProfileLevelCurrent,
      &profile_level);
  CHECK_RESULT("set heic profile:", result);

  // check set parameter result:
  OMX_INIT_STRUCT(&profile_level, OMX_VIDEO_PARAM_PROFILELEVELTYPE);
  result = OMX_GetParameter(m_Handle,
      OMX_IndexParamVideoProfileLevelCurrent,
      &profile_level);
  CHECK_RESULT("get heic set profile result:", result);
  VLOGD("get heic real Profile = %d level = %d",
      profile_level.eProfile, profile_level.eLevel);

  OMX_VIDEO_PARAM_HEVCTYPE heic;
  OMX_INIT_STRUCT(&heic, OMX_VIDEO_PARAM_HEVCTYPE);
  heic.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
  result = OMX_GetParameter(m_Handle,
      (OMX_INDEXTYPE)OMX_IndexParamVideoHevc, (OMX_PTR)&heic);
  CHECK_RESULT("get HEIC default parameter:", result);

  heic.eProfile = (OMX_VIDEO_HEVCPROFILETYPE)user_profile;
  heic.eLevel = (OMX_VIDEO_HEVCLEVELTYPE)e_level;
  result = OMX_SetParameter(m_Handle,
      (OMX_INDEXTYPE)OMX_IndexParamVideoHevc, (OMX_PTR)&heic);
  CHECK_RESULT("set heic parameter:", result);

  result = OMX_GetParameter(m_Handle,
      (OMX_INDEXTYPE)OMX_IndexParamVideoHevc,
      &heic);
  CHECK_RESULT("get heic parameter:", result);

  OMX_VIDEO_PARAM_ANDROID_IMAGEGRIDTYPE imagegriddata;
  OMX_INIT_STRUCT(&imagegriddata, OMX_VIDEO_PARAM_ANDROID_IMAGEGRIDTYPE);
  imagegriddata.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  imagegriddata.bEnabled = OMX_FALSE;
  imagegriddata.nTileWidth = DEFAULT_TILE_DIMENSION;
  imagegriddata.nTileHeight = DEFAULT_TILE_DIMENSION;
  imagegriddata.nGridRows = DEFAULT_TILE_COUNT;
  imagegriddata.nGridRows = DEFAULT_TILE_COUNT;

  result = OMX_SetParameter(m_Handle,
      (OMX_INDEXTYPE)OMX_IndexParamVideoAndroidImageGrid,
      &imagegriddata);
  CHECK_RESULT("set heic parameter:", result);

  FUNCTION_EXIT();
  return true;
}

bool SetBitRateAndEControlRate(OMX_U32 bitrate, int32_t controlrate) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  bool set_controlrate = false;

  FUNCTION_ENTER();

  VLOGD("nBitrate:%d, eControlRate:%d", bitrate, controlrate);

  if (controlrate == OMX_Video_ControlRateDisable) {
    result = SetVendorRateControlMode(
        (OMX_VIDEO_CONTROLRATETYPE)controlrate);
    CHECK_RESULT("set control rate disable failed", result);

    FUNCTION_EXIT();
    return false;
  }

  if (bitrate != DEADVALUE &&
      (controlrate == OMX_Video_ControlRateVariableSkipFrames ||
       controlrate == OMX_Video_ControlRateConstantSkipFrames)) {
    result = SetVendorRateControlMode(
        (OMX_VIDEO_CONTROLRATETYPE)controlrate);
    CHECK_RESULT("set control rate skip failed", result);

    set_controlrate = false;
  } else if (bitrate != DEADVALUE) {
    set_controlrate = true;
  } else {
    VLOGE("bad parameter: bitrate:%d, eControlRate:%d",
        bitrate, controlrate);

    FUNCTION_EXIT();
    return false;
  }

  OMX_VIDEO_PARAM_BITRATETYPE bitrate_type;

  OMX_INIT_STRUCT(&bitrate_type, OMX_VIDEO_PARAM_BITRATETYPE);
  bitrate_type.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
  result = OMX_GetParameter(m_Handle,
      OMX_IndexParamVideoBitrate, (OMX_PTR)&bitrate_type);
  CHECK_RESULT("get video bit rate default parameter:", result);

  if (set_controlrate) {
    bitrate_type.eControlRate = (OMX_VIDEO_CONTROLRATETYPE)controlrate;  // ControlRate
  }

  bitrate_type.nTargetBitrate = bitrate;
  result = OMX_SetParameter(m_Handle,
      OMX_IndexParamVideoBitrate, (OMX_PTR)&bitrate_type);
  CHECK_RESULT("set video bit rate:", result);

  FUNCTION_EXIT();
  return true;
}

bool EnablePrependSPSPPSToIDRFrame(int32_t enable) {
  OMX_ERRORTYPE result = OMX_ErrorNone;

  FUNCTION_ENTER();

  VLOGD("enable:%d", enable);

  PrependSPSPPSToIDRFramesParams param;
  OMX_INIT_STRUCT(&param, PrependSPSPPSToIDRFramesParams);
  param.bEnable = (OMX_BOOL)enable;  // default: OMX_FALSE
  result = OMX_SetParameter(m_Handle,
      (OMX_INDEXTYPE)OMX_QcomIndexParamSequenceHeaderWithIDR,
      (OMX_PTR)&param);
  CHECK_RESULT("set SPS PPS to IDR flag:", result);

  FUNCTION_EXIT();
  return true;
}

bool SetIntraPeriod(OMX_U32 idr_period, OMX_U32 pframes, OMX_U32 bframes) {
  OMX_ERRORTYPE result = OMX_ErrorNone;

  FUNCTION_ENTER();

  VLOGD("IDR period:%d, pFrame:%d, bFrame: %d",
      idr_period, pframes, bframes);

  QOMX_VIDEO_INTRAPERIODTYPE intra;
  OMX_INIT_STRUCT(&intra, QOMX_VIDEO_INTRAPERIODTYPE);
  intra.nPortIndex = (OMX_U32) PORT_INDEX_OUT;  // output
  result = OMX_GetConfig(m_Handle,
      (OMX_INDEXTYPE) QOMX_IndexConfigVideoIntraperiod,
      (OMX_PTR) &intra);
  CHECK_RESULT("get intra period default parameter:", result);

  // setting I frame interval to 2 x framerate
  if (pframes != DEADVALUE) {
    intra.nPFrames = pframes;
  }
  if (bframes != DEADVALUE && bframes != 0) {
    intra.nPFrames /= (intra.nBFrames +1);
  }
  if (bframes != DEADVALUE) {
    intra.nBFrames = bframes;
  }
  intra.nIDRPeriod = idr_period; // default is 1: every I frame is an IDR
  intra.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  result = OMX_SetConfig(m_Handle,
      (OMX_INDEXTYPE) QOMX_IndexConfigVideoIntraperiod,
      (OMX_PTR) &intra);
  CHECK_RESULT("set intra preriod failed:", result);

  FUNCTION_EXIT();
  return true;
}

bool SetAVCIntraPeriod(OMX_U32 idr_period, OMX_U32 pframes) {
  OMX_ERRORTYPE result = OMX_ErrorNone;

  FUNCTION_ENTER();

  VLOGD("IDR period:%d, pFrame:%d", idr_period, pframes);

  OMX_VIDEO_CONFIG_AVCINTRAPERIOD config_avcintraperiod;
  OMX_INIT_STRUCT(&config_avcintraperiod, OMX_VIDEO_CONFIG_AVCINTRAPERIOD);
  config_avcintraperiod.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  result = OMX_GetConfig(m_Handle,
      (OMX_INDEXTYPE) OMX_IndexConfigVideoAVCIntraPeriod,
      (OMX_PTR) &config_avcintraperiod);
  CHECK_RESULT("get avc intra period default parameter:", result);

  config_avcintraperiod.nIDRPeriod = idr_period;
  config_avcintraperiod.nPFrames = 268435455;
  result = OMX_SetConfig(m_Handle,
      (OMX_INDEXTYPE) OMX_IndexConfigVideoAVCIntraPeriod,
      (OMX_PTR) &config_avcintraperiod);
  CHECK_RESULT("set avc intra preriod failed:", result);

  FUNCTION_EXIT();
  return true;
}

bool ConfigErrorCorrection(OMX_U32 codec,
    OMX_U32 resyncmarker_type,
    OMX_U32 resyncmarker_spacing,
    OMX_U32 hecinterval) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_BOOL enable_hec = OMX_FALSE;

  FUNCTION_ENTER();

  VLOGD("eCodec:%d, MarkerType:%d, MarkerSpacing: %d, HECinternal:%d",
      codec, resyncmarker_type, resyncmarker_spacing,
      hecinterval);

  OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE error_correction;
  OMX_INIT_STRUCT(&error_correction, OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE);
  error_correction.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  result = OMX_GetParameter(m_Handle,
      (OMX_INDEXTYPE) OMX_IndexParamVideoErrorCorrection,
      (OMX_PTR) &error_correction);
  CHECK_RESULT("get video error correction default parameter:", result);

  error_correction.bEnableRVLC = OMX_FALSE;
  error_correction.bEnableDataPartitioning = OMX_FALSE;

  if (resyncmarker_type == RESYNC_MARKER_MB) {
    switch (codec) {
      case OMX_VIDEO_CodingAVC:
      case OMX_VIDEO_CodingHEVC:
        /* Configure Slice Spacing count */
        QOMX_VIDEO_PARAM_SLICE_SPACING_TYPE  slice_spacing;
        OMX_INIT_STRUCT(&slice_spacing, QOMX_VIDEO_PARAM_SLICE_SPACING_TYPE);
        slice_spacing.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
        slice_spacing.eSliceMode = (QOMX_VIDEO_SLICEMODETYPE)resyncmarker_type;
        slice_spacing.nSliceSize = resyncmarker_spacing;

        VLOGD("Configure slice mode %u with slice size = %u",
            (unsigned int)resyncmarker_type,
            (unsigned int)resyncmarker_spacing);

        result = OMX_SetParameter(m_Handle,
            (OMX_INDEXTYPE)OMX_QcomIndexParamVideoSliceSpacing,
            (OMX_PTR)&slice_spacing);
        CHECK_RESULT("Set Slice Spacing failed:", result);

        break;
      case OMX_VIDEO_CodingMPEG4:
        OMX_VIDEO_PARAM_MPEG4TYPE mp4;

        OMX_INIT_STRUCT(&mp4, OMX_VIDEO_PARAM_MPEG4TYPE);
        mp4.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
        result = OMX_GetParameter(m_Handle,
            OMX_IndexParamVideoMpeg4,
            (OMX_PTR) &mp4);
        CHECK_RESULT("get MPEG4 type default parameter:", result);

        if (resyncmarker_spacing != 0) {
          mp4.nSliceHeaderSpacing = resyncmarker_spacing;
        } else {
          mp4.nSliceHeaderSpacing = 50;
        }
        result = OMX_SetParameter(m_Handle,
            OMX_IndexParamVideoMpeg4,
            (OMX_PTR) &mp4);
        CHECK_RESULT("set MPEG4 type parameter:", result);
        break;
      default:
        VLOGE("Do not support codec:0x%x in type: MARKER_MB", codec);
        FUNCTION_EXIT();
        return false;
    }
  } else if (resyncmarker_type != RESYNC_MARKER_NONE) {
    switch (codec) {
      case OMX_VIDEO_CodingMPEG4:
        if (resyncmarker_type == RESYNC_MARKER_BYTE) {
          error_correction.bEnableResync = OMX_TRUE;
          if (resyncmarker_spacing != 0) {
            error_correction.nResynchMarkerSpacing = resyncmarker_spacing;
          } else {
            error_correction.nResynchMarkerSpacing = 1920;
          }
          error_correction.bEnableHEC = hecinterval == 0 ? OMX_FALSE : OMX_TRUE;
        }
        break;
      case OMX_VIDEO_CodingH263:
        if (resyncmarker_type == RESYNC_MARKER_GOB) {
          error_correction.bEnableResync = OMX_FALSE;
          error_correction.nResynchMarkerSpacing = resyncmarker_spacing;
          error_correction.bEnableDataPartitioning = OMX_TRUE;
        }
        break;
      case OMX_VIDEO_CodingAVC:
      case OMX_VIDEO_CodingHEVC:
        if (resyncmarker_type == RESYNC_MARKER_BYTE) {
          if (resyncmarker_spacing < 512 || resyncmarker_spacing > 500000) {
            VLOGE("invlide slice size for HEVC.");
            FUNCTION_EXIT();
            return false;
          }
          error_correction.bEnableResync = OMX_TRUE;
          error_correction.nResynchMarkerSpacing = resyncmarker_spacing << 3;
        }
        break;
      default:
        VLOGE("Do not support eCodec:0x%x", codec);
        FUNCTION_EXIT();
        return false;
    }

    VLOGD("nResynchMarkerSpacing: %d, but rel is :%d", resyncmarker_spacing, error_correction.nResynchMarkerSpacing);
    OMX_INIT_STRUCT_SIZE(&error_correction, OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE);
    result = OMX_SetParameter(m_Handle,
        (OMX_INDEXTYPE) OMX_IndexParamVideoErrorCorrection,
        (OMX_PTR) &error_correction);
    CHECK_RESULT("set video error correction failed:", result);
  }

  FUNCTION_EXIT();
  return true;
}

bool SetIntraRefresh(OMX_U32 width, OMX_U32 height, int32_t refresh_mode, OMX_U32 intra_refresh) {
  OMX_ERRORTYPE result = OMX_ErrorNone;

  FUNCTION_ENTER();

  VLOGD("nRefreshMode:%d, nIntraRefresh:%d", refresh_mode, intra_refresh);

  if (intra_refresh <= ((width * height) >> 8)) {
    if (OMX_VIDEO_IntraRefreshRandom != (OMX_VIDEO_INTRAREFRESHTYPE)refresh_mode) {
      OMX_VIDEO_PARAM_INTRAREFRESHTYPE ir;
      OMX_INIT_STRUCT(&ir, OMX_VIDEO_PARAM_INTRAREFRESHTYPE);
      ir.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
      result = OMX_GetParameter(m_Handle,
          OMX_IndexParamVideoIntraRefresh,
          (OMX_PTR) &ir);
      CHECK_RESULT("get intra refresh default failed:", result);
      ir.eRefreshMode = (OMX_VIDEO_INTRAREFRESHTYPE)refresh_mode;
      if (refresh_mode == OMX_VIDEO_IntraRefreshAdaptive) {
        ir.nAirMBs = intra_refresh;
      } else {
        ir.nCirMBs = intra_refresh;
      }
      result = OMX_SetParameter(m_Handle, OMX_IndexParamVideoIntraRefresh, (OMX_PTR) &ir);
      CHECK_RESULT("SetConfig for Cyclic IntraRefresh returned error:", result);
    }
#ifdef SUPPORT_CONFIG_INTRA_REFRESH
    else {
      OMX_VIDEO_CONFIG_ANDROID_INTRAREFRESHTYPE ir;
      OMX_U32 mb_size = 16;
      OMX_U32 num_mbs_per_frame = (ALIGN(height, mb_size)/mb_size) * (ALIGN(width, mb_size)/mb_size);
      OMX_U32 refresh_period = 0;
      OMX_INIT_STRUCT(&ir, OMX_VIDEO_CONFIG_ANDROID_INTRAREFRESHTYPE);
      ir.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
      if (intra_refresh == 0) {
        refresh_period = ir.nRefreshPeriod = 0;
      } else if (intra_refresh == ((width * height) >> 8)) {
        refresh_period = ir.nRefreshPeriod = 1;
      } else {
        refresh_period = ir.nRefreshPeriod = ceil(num_mbs_per_frame / intra_refresh);
      }
      result = OMX_SetConfig(m_Handle,
          (OMX_INDEXTYPE)OMX_IndexConfigAndroidIntraRefresh, (OMX_PTR)&ir);
      CHECK_RESULT("SetConfig for Android IntraRefresh returned error:", result);

      result = OMX_GetConfig(m_Handle,
          (OMX_INDEXTYPE)OMX_IndexConfigAndroidIntraRefresh, (OMX_PTR)&ir);
      CHECK_RESULT("GetConfig for IntraRefresh returned error:", result);

      if (refresh_period != ir.nRefreshPeriod) {
        VLOGE("Failed to read back correct IntraRefresh Period Set(%d) vs Read(%d)",
            refresh_period, ir.nRefreshPeriod);
        return OMX_ErrorInvalidState;
      }
    }
#endif
  } else {
    VLOGE("IntraRefresh not set for %u MBs because total MBs is %u", (unsigned int)intra_refresh,
        (unsigned int)(width * height >> 8));
  }

  FUNCTION_EXIT();

  return true;
}

bool SetFramePackingData(const char* config_file_name) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_QCOM_FRAME_PACK_ARRANGEMENT frame_packing_arrangement;
  OMX_INIT_STRUCT(&frame_packing_arrangement, OMX_QCOM_FRAME_PACK_ARRANGEMENT);
  frame_packing_arrangement.nPortIndex = (OMX_U32)PORT_INDEX_OUT;

  FUNCTION_ENTER();

  if (config_file_name != NULL) {
    FILE *p_configfile;
    p_configfile = fopen(config_file_name, "r");
    int total_size_read = FRAME_PACK_SIZE * sizeof(OMX_U32);
    if (p_configfile != NULL) {
      OMX_U32 *p_framepack = reinterpret_cast<OMX_U32 *>(&(frame_packing_arrangement.id));
      while ( ( (fscanf(p_configfile, "%d", p_framepack)) != EOF ) &&
          (total_size_read != 0) ) {
        p_framepack += sizeof(OMX_U32);
        total_size_read -= sizeof(OMX_U32);
      }

      fclose(p_configfile);

      VLOGD("Dump Frame Packing data, read from config file: %s",
          config_file_name);
      DumpFramePackArrangement(frame_packing_arrangement);

      if (total_size_read != 0) {
        VLOGE("The config file: %s, missing: %d bytes",
            config_file_name, total_size_read);
        return false;
      }
    } else {
      VLOGE("Open Frame pack config file:%s failed:%s",
          config_file_name, strerror(errno));
      return false;
    }
  } else {  // use default frame pack data
    frame_packing_arrangement.nSize = sizeof(OMX_QCOM_FRAME_PACK_ARRANGEMENT);
    frame_packing_arrangement.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
    frame_packing_arrangement.id = 123;
    frame_packing_arrangement.cancel_flag = false;
    frame_packing_arrangement.type = 3;
    frame_packing_arrangement.quincunx_sampling_flag = false;
    frame_packing_arrangement.content_interpretation_type = 0;
    frame_packing_arrangement.spatial_flipping_flag = true;
    frame_packing_arrangement.frame0_flipped_flag = false;
    frame_packing_arrangement.field_views_flag = false;
    frame_packing_arrangement.current_frame_is_frame0_flag = false;
    frame_packing_arrangement.frame0_self_contained_flag = true;
    frame_packing_arrangement.frame1_self_contained_flag = false;
    frame_packing_arrangement.frame0_grid_position_x = 3;
    frame_packing_arrangement.frame0_grid_position_y = 15;
    frame_packing_arrangement.frame1_grid_position_x = 11;
    frame_packing_arrangement.frame1_grid_position_y = 7;
    frame_packing_arrangement.reserved_byte = 0;
    frame_packing_arrangement.repetition_period = 16381;
    frame_packing_arrangement.extension_flag = false;

    VLOGD("Dump default Frame Packing data:");
    DumpFramePackArrangement(frame_packing_arrangement);
  }

  result = OMX_SetConfig(m_Handle,
      (OMX_INDEXTYPE)OMX_QcomIndexConfigVideoFramePackingArrangement,
      (OMX_PTR) &frame_packing_arrangement);
  CHECK_RESULT("set frame packing data failed:", result);

  FUNCTION_EXIT();
  return true;
}

bool SetFrameRate(OMX_U32 framerate) {
  OMX_ERRORTYPE result = OMX_ErrorNone;

  FUNCTION_ENTER();
  VLOGD("set new frame rate: %d", framerate);

  OMX_CONFIG_FRAMERATETYPE enc_framerate;
  OMX_INIT_STRUCT(&enc_framerate, OMX_CONFIG_FRAMERATETYPE);
  enc_framerate.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
  result = OMX_GetConfig(m_Handle,
      OMX_IndexConfigVideoFramerate,
      &enc_framerate);
  CHECK_RESULT("get default frame rate failed:", result);

  FractionToQ16(enc_framerate.xEncodeFramerate, framerate);
  result = OMX_SetConfig(m_Handle,
      OMX_IndexConfigVideoFramerate,
      &enc_framerate);
  CHECK_RESULT("set frame rate failed:", result);

  if (result == OMX_ErrorNone) {
    m_FrameTime = 1000000 / framerate;
  }

  FUNCTION_EXIT();
  return true;
}

bool SetFrameMirror(OMX_U32 mirror) {
  OMX_ERRORTYPE result = OMX_ErrorNone;

  FUNCTION_ENTER();
  VLOGD("set mirror: %d", mirror);

  if (mirror == 0) {
    VLOGD("nMirror == 0, skip mirror");
    FUNCTION_EXIT();
    return true;
  }

  OMX_CONFIG_MIRRORTYPE enc_mirror;
  OMX_INIT_STRUCT(&enc_mirror, OMX_CONFIG_MIRRORTYPE);
  enc_mirror.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
  result = OMX_GetConfig(m_Handle,
      OMX_IndexConfigCommonMirror,
      &enc_mirror);
  CHECK_RESULT("get default mirror failed:", result);

  enc_mirror.eMirror = (OMX_MIRRORTYPE)mirror;
  result = OMX_SetConfig(m_Handle,
      OMX_IndexConfigCommonMirror,
      &enc_mirror);
  CHECK_RESULT("set mirror failed:", result);

  FUNCTION_EXIT();
  return true;
}

bool SetFrameRotation(OMX_U32 rotation) {
  OMX_ERRORTYPE result = OMX_ErrorNone;

  FUNCTION_ENTER();

  VLOGD("nRotation: %d", rotation);

  if (rotation == 0) {
    VLOGD("nRotation == 0, skip set rotation");
    FUNCTION_EXIT();
    return true;
  }

  OMX_CONFIG_ROTATIONTYPE framerotate;
  OMX_INIT_STRUCT(&framerotate, OMX_CONFIG_ROTATIONTYPE);
  framerotate.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
  result = OMX_GetConfig(m_Handle,
      OMX_IndexConfigCommonRotate, (OMX_PTR)&framerotate);
  CHECK_RESULT("get rotation default value failed", result);

  framerotate.nRotation = rotation;
  result = OMX_SetConfig(m_Handle,
      OMX_IndexConfigCommonRotate, (OMX_PTR)&framerotate);
  CHECK_RESULT("set rotation default value failed", result);

  FUNCTION_EXIT();
  return true;
}

bool SetFrameScale(uint32_t height, uint32_t width, uint32_t crop_height, uint32_t crop_width){
  OMX_ERRORTYPE result = OMX_ErrorNone;

  FUNCTION_ENTER();

  VLOGD("scale to : %d x %d", width, height);

  if (crop_width != 0 && crop_height != 0) {
    width = crop_width;
    height = crop_height;
  }

  if (width == 0 || height == 0) {
    VLOGD("width/height == 0, skip scale");
    FUNCTION_EXIT();
    return true;
  }

  QOMX_INDEXDOWNSCALAR framedownscalar;
  OMX_INIT_STRUCT(&framedownscalar, QOMX_INDEXDOWNSCALAR);
  framedownscalar.nPortIndex = (OMX_U32)PORT_INDEX_OUT;

  framedownscalar.bEnable = OMX_TRUE;
  framedownscalar.nOutputWidth = (OMX_U32)width;
  framedownscalar.nOutputHeight = (OMX_U32)height;

  result = OMX_SetParameter(m_Handle,
      (OMX_INDEXTYPE)OMX_QcomIndexParamVideoDownScalar, (OMX_PTR)&framedownscalar);
  CHECK_RESULT("scale failed", result);

  FUNCTION_EXIT();
  return true;
}

const char * TransferStateToStr(OMX_U32 state) {
  switch (state) {
    case OMX_StateLoaded:
      return "OMX_StateLoaded";
    case OMX_StateIdle:
      return "OMX_StateIdle";
    case OMX_StateExecuting:
      return "OMX_StateExecuting";
    case OMX_StateInvalid:
      return "OMX_StateInvalid";
    case OMX_StateWaitForResources:
      return "OMX_StateWaitForResources";
    case OMX_StatePause:
      return "OMX_StatePause";
    default:
      return "Not Support this state";
  }
}


bool WaitState(OMX_STATETYPE state) {
  FUNCTION_ENTER();

  VLOGD("waiting state: %s", TransferStateToStr(state));

  // wait component return set result
  StateEvent_t new_state;
  bool ret = PopEvent(&new_state);
  if (ret == false) {
    VLOGE("Waiting for state:%s failed", TransferStateToStr(state));
    FUNCTION_EXIT();
    return false;
  }

  if (new_state.event == OMX_EventCmdComplete &&
      new_state.cmd == OMX_CommandStateSet &&
      new_state.cmd_data == state) {
    VLOGD("wait state: %s Success", TransferStateToStr(state));
    m_State = state;
    ret = true;
  } else {
    VLOGE("recv bad event: %d, cmd: %d, cmdData: %d",
        new_state.event,
        new_state.cmd,
        new_state.cmd_data);
    ret = false;
  }

  FUNCTION_EXIT();
  return ret;
}

/*
 * Set new state to OMX component and wait component result.
 *
 * Max wait time is setting in PopEvent(currently is 1s).
 * If timeout or return state is not same as request, then return false.
 */
bool SetState(OMX_STATETYPE state, OMX_BOOL sync) {
  OMX_ERRORTYPE result = OMX_ErrorUndefined;
  bool ret = true;

  FUNCTION_ENTER();

  VLOGD("current:%s, pending: %s, new: %s, sync: %d",
      TransferStateToStr(m_State),
      TransferStateToStr(m_PendingEstate),
      TransferStateToStr(state),
      sync);

  if (m_State == state) {
    VLOGD("set old state:%s, skip", TransferStateToStr(state));
    FUNCTION_EXIT();
    return true;
  }

  if (m_PendingEstate != m_State) {
    ret = WaitState(m_PendingEstate);
    CHECK_BOOL("waitState failed", ret);  // if ret == false, exit
  }

  if ((state == OMX_StateLoaded && m_State != OMX_StateIdle) ||
      (state == OMX_StateExecuting && m_State != OMX_StateIdle)) {
    VLOGE("Invalid new state: %d, current state: %d",
        state, m_State);
    FUNCTION_EXIT();
    return false;
  }

  // send new state to component
  VLOGD("Goto new state: %s, sync:%d", TransferStateToStr(state), sync);
  result = OMX_SendCommand(m_Handle, OMX_CommandStateSet,
      (OMX_U32) state, NULL);
  CHECK_RESULT("set new state failed:", result);

  m_PendingEstate = state;

  if (sync == true) {
    ret = WaitState(m_PendingEstate);
    CHECK_BOOL("wait sync state failed", ret);
  }

  FUNCTION_EXIT();
  return ret;
}

static uint32_t m_InputFrameNum = 0;
static uint32_t m_FillFramNum = 0;
static uint32_t m_OutputFrameNum = 0;

static pthread_t m_InputProcessThreadId;
static bool m_InputThreadRunning = false;
#define MAX_WAIT_TIMES 5

static void * InputBufferProcessThread(void *arg) {
  int32_t i = 0;
  int32_t read_frame_length = 0;
  OMX_ERRORTYPE result = OMX_ErrorUndefined;
  bool ret = false;
  int32_t retry = 0;

  FUNCTION_ENTER();

  m_InputThreadRunning = true;

  while (m_InputThreadRunning == true && retry < MAX_WAIT_TIMES) {
    BufferMessage_t buffer_msg;

    if (PopInputEmptyBuffer(&buffer_msg) == false) {
      retry++;
      continue;
    }

    ProcessDynamicConfiguration(m_InputFrameNum);
    for (i = 0; i < m_InputBufferCount; i++) {
      if (buffer_msg.p_buffer == m_InputBufferHeaders[i]) {
        read_frame_length = 0;

        if (m_MetadataMode) {
          MetaBuffer *p_meta_buffer = (MetaBuffer *)buffer_msg.p_buffer->pBuffer;
          // CameraSource
          // int fd = pMetaBuffer->meta_handle->data[0];
          // int offset = pMetaBuffer->meta_handle->data[1];
          // int size = pMetaBuffer->meta_handle->data[2];

          // if (0 >= fd) {
          //     VLOGE("Get fd failed: %d, frame: %d", fd, m_InputFrameNum);
          //     m_InputThreadRunning = false;
          //     NotifyToStop();
          //     break;
          // }
          // char buf[1024] = {'\0'};
          // char file_path[size] = {'0'};
          // snprintf(buf, sizeof (buf), "/proc/self/fd/%d", fd);
          // readlink(buf, file_path, sizeof(file_path) - 1);
          // GetNextFrame((unsigned char*)file_path + offset, &read_frame_length);
          // GrallocSource
          VLOGD("use address: %p", p_meta_buffer->meta_handle->base_metadata);
          GetNextFrame((unsigned char*)p_meta_buffer->meta_handle->base_metadata, &read_frame_length);
        } else {
          GetNextFrame(buffer_msg.p_buffer->pBuffer, &read_frame_length);
        }
        if (read_frame_length == 0) {
          VLOGE("Get buffer failed: %d, frame: %d", read_frame_length,
              m_InputFrameNum);
          m_InputThreadRunning = false;
          // NotifyToStop();
          break;  // read file finished, so thread exit
        }

        buffer_msg.p_buffer->nFilledLen = read_frame_length;

        /* if control fps by encode, maybe need waiting, start.*/
        if (m_ControlFps && m_InputFrameNum != 0) {
          struct timeval encode_frame_start_time =
            m_EncodeFrameStartTime[(m_InputFrameNum - 1) % (OMX_BUFFERS_NUM * 2)];
          if (0 != m_FrameTime) {
            struct timeval now;
            gettimeofday(&now, NULL);
            int64_t diff_time = (now.tv_sec - encode_frame_start_time.tv_sec) * 1000 + \
                                (now.tv_usec - encode_frame_start_time.tv_usec) / 1000;
            bool need_waiting = diff_time < m_FrameTime / 1000;
            if (need_waiting) {
              VLOGD("waiting to encode next frame ---");
              usleep(m_FrameTime/1000 - diff_time);
            }
          }
        }

        gettimeofday(&m_EncodeFrameStartTime[(m_InputFrameNum) % (OMX_BUFFERS_NUM * 2)], NULL);
        /* End. */

        timeval start = m_EncodeFrameStartTime[m_InputFrameNum % (OMX_BUFFERS_NUM * 2)];

        m_InputBufferHeaders[i]->nTimeStamp = m_TimeStamp;
        result = OMX_EmptyThisBuffer(m_Handle, m_InputBufferHeaders[i]);
        // if failed, then thread exit
        if ((result != OMX_ErrorNone) && (result != OMX_ErrorNoMore)) {
          VLOGE("request empty buffer failed, exit");
          NotifyToStop();
          break;
        }
        m_TimeStamp += m_FrameTime;
        m_InputFrameNum++;
        VLOGD("start encoding frame: %d \n\n", m_InputFrameNum);
      }
    }
  }

  VLOGD("input flag: %d, frame num: %d, retry: %d",
      m_InputThreadRunning, m_InputFrameNum, retry);
  FUNCTION_EXIT();

  return 0;
}

static pthread_t m_OutPutProcessThreadId;
static bool m_OutputThreadRunning = false;
static void * OutputBufferProcessThread(void *arg) {
  bool ret = false;
  OMX_ERRORTYPE result = OMX_ErrorUndefined;
  int32_t retry = 0;
  int32_t i = 0;

  FUNCTION_ENTER();

  m_OutputThreadRunning = true;

  while (retry < MAX_WAIT_TIMES) {
    BufferMessage_t buffer_msg;

    if (PopOutputFilledBuffer(&buffer_msg) == false) {
      retry++;
      continue;
    }

    retry = 0;
    for (i = 0; i < m_OutputBufferCount; i++) {
      if (buffer_msg.p_buffer == m_OutputBufferHeaders[i]) {
        break;
      }
    }

    if (i >= m_OutputBufferCount) {
      VLOGE("recv invalid buffer: 0x%p, len: %d",
          buffer_msg.p_buffer->pBuffer,
          buffer_msg.p_buffer->nFilledLen);
      continue;
    }

    // TODO: debug
    if (buffer_msg.p_buffer->nFilledLen == 0) {
      result = OMX_FillThisBuffer(m_Handle, buffer_msg.p_buffer);
      VLOGE("skip len == 0 buffer");
      continue;
    }

    m_OutputFrameNum++;
    VLOGD("recv encoded frame: %d, len: %d, buffer index:%d\n\n",
        m_OutputFrameNum, buffer_msg.p_buffer->nFilledLen, i);

    ret = StoreEncodedData(buffer_msg.p_buffer->pBuffer + buffer_msg.p_buffer->nOffset,
        buffer_msg.p_buffer->nFilledLen, buffer_msg.p_buffer->nTimeStamp);
    if (ret == false) {
      VLOGE("Store encode data:%d, size:%d failed, exit",
          m_OutputFrameNum, buffer_msg.p_buffer->nFilledLen);
      m_OutputThreadRunning = false;
      NotifyToStop();
      break;
    }

    result = OMX_FillThisBuffer(m_Handle, buffer_msg.p_buffer);
    if ((result != OMX_ErrorNone) && (result != OMX_ErrorNoMore)) {
      VLOGE("request fill buffer failed, exit");
      m_OutputThreadRunning = false;
      NotifyToStop();
      break;
    }
  }

  VLOGD("output flag: %d, frame num: %d, retry: %d",
      m_OutputThreadRunning, m_OutputFrameNum, retry);
  NotifyToStop();
  FUNCTION_EXIT();

  return 0;
}

bool StartProcessThread() {
  bool ret = false;

  FUNCTION_ENTER();

  if (pthread_create(&m_OutPutProcessThreadId, NULL,
        OutputBufferProcessThread, NULL)) {
    VLOGE("create output event process thread failed: %s", strerror(errno));
    FUNCTION_EXIT();
    return false;
  }

  if (pthread_create(&m_InputProcessThreadId, NULL,
        InputBufferProcessThread, NULL)) {
    VLOGE("create input event process thread failed: %s", strerror(errno));
    FUNCTION_EXIT();
    return false;
  }

  FUNCTION_EXIT();
  return true;
}

void WaitProcessThreadExit() {
  pthread_join(m_InputProcessThreadId, NULL);
  pthread_join(m_OutPutProcessThreadId, NULL);
}

OMX_ERRORTYPE EventCallback(OMX_IN OMX_HANDLETYPE component,
    OMX_IN OMX_PTR appdata,
    OMX_IN OMX_EVENTTYPE event,
    OMX_IN OMX_U32 data1,
    OMX_IN OMX_U32 data2,
    OMX_IN OMX_PTR p_event_data) {
  OMX_ERRORTYPE result = OMX_ErrorNone;

  FUNCTION_ENTER();
  VLOGD("Recv event: %d, data1: %d, data2: %d", event, data1, data2);

  if (event == OMX_EventError) {
    VLOGE("Recv error event, goto stop");
    NotifyToStop();
    FUNCTION_EXIT();
    return result;
  }

  StateEvent_t new_state;
  new_state.event = event;
  new_state.cmd = data1;
  new_state.cmd_data = data2;
  PushEvent(new_state);

  FUNCTION_EXIT();
  return result;
}

OMX_ERRORTYPE EmptyBufferDoneCallback(OMX_IN OMX_HANDLETYPE component,
    OMX_IN OMX_PTR appdata,
    OMX_IN OMX_BUFFERHEADERTYPE* p_buffer) {
  FUNCTION_ENTER();

  BufferMessage_t buffer_msg;
  buffer_msg.p_buffer = p_buffer;

  PushInputEmptyBuffer(buffer_msg);
  VLOGD("EncodeFrameEnd");

  FUNCTION_EXIT();
  return OMX_ErrorNone;
}

OMX_ERRORTYPE FillBufferDoneCallback(OMX_OUT OMX_HANDLETYPE component,
    OMX_OUT OMX_PTR appdata,
    OMX_OUT OMX_BUFFERHEADERTYPE* p_buffer) {
  FUNCTION_ENTER();
  VLOGD("end encode frame done:%d", m_FillFramNum);

  if (!m_OutputThreadRunning) {
    FUNCTION_EXIT();
    return OMX_ErrorNone;
  }

  gettimeofday(&m_EncodeFrameEndTime[m_FillFramNum % (OMX_BUFFERS_NUM * 2)], NULL);

  timeval encode_frame_start_time = m_EncodeFrameStartTime[m_FillFramNum % (OMX_BUFFERS_NUM * 2)];
  timeval encode_frame_end_time = m_EncodeFrameEndTime[m_FillFramNum % (OMX_BUFFERS_NUM * 2)];

  if (encode_frame_start_time.tv_sec != 0) {
    int64_t encode_frame_time = (encode_frame_end_time.tv_sec - encode_frame_start_time.tv_sec) * 1000 + \
                                (encode_frame_end_time.tv_usec - encode_frame_start_time.tv_usec) / 1000;

    if (m_EncodeFrameTimeMax < encode_frame_time) {
      m_EncodeFrameTimeMax = encode_frame_time;
    }
    if (m_EncodeFrameTimeMin > encode_frame_time || m_EncodeFrameTimeMin == 0) {
      m_EncodeFrameTimeMin = encode_frame_time;
    }
    VLOGP("encode frame speed max: %5d ms, min %5d ms", m_EncodeFrameTimeMax, m_EncodeFrameTimeMin);

    m_EncodeTotalTimeActal += encode_frame_time;
  }
  m_FillFramNum++;

  BufferMessage_t buffer_msg;
  buffer_msg.p_buffer = p_buffer;
  PushOutputFilledBuffer(buffer_msg);

  FUNCTION_EXIT();
  return OMX_ErrorNone;
}

bool RegisterBuffer(int32_t height, int32_t width, int colorformat, int codec) {
  int32_t i = 0;
  unsigned char * buffer = NULL;
  OMX_ERRORTYPE result = OMX_ErrorNone;

  FUNCTION_ENTER();

  if (m_InputBufferSize == 0 ||
      m_InputBufferCount == 0 ||
      m_OutputBufferSize == 0 ||
      m_OutputBufferCount == 0 ||
      m_InputBufferCount > OMX_BUFFERS_NUM ||
      m_OutputBufferCount > OMX_BUFFERS_NUM) {
    VLOGE("Invalid buffer parameter: input:%d/%d, output:%d/%d, count:%d",
        m_InputBufferSize, m_InputBufferCount,
        m_OutputBufferSize, m_OutputBufferCount,
        OMX_BUFFERS_NUM);
    FUNCTION_EXIT();
    return false;
  }

  bool ubwc_flags = (colorformat == QOMX_COLOR_FORMATYUV420PackedSemiPlanar32mCompressed ||
      colorformat == QOMX_COLOR_Format32bitRGBA8888Compressed ||
      colorformat == QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m10bitCompressed);

  // alloc input buffer
  m_IonDataArray = (struct EncodeIon *)calloc(sizeof(struct EncodeIon), m_InputBufferCount);
  int frame_size = VENUS_BUFFER_SIZE_USED((codec == OMX_VIDEO_CodingImageHEIC ?
        COLOR_FMT_NV12_512 : ConvertColorFomat(colorformat)), width, height, true);
  m_MetaFrameBufferSize = frame_size;

  for (i = 0; i < m_InputBufferCount; i++) {
    OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO* p_inputmem = new OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO;

    if (!m_MetadataMode) {
      buffer = (OMX_U8*)AllocVideoBuffer(p_inputmem, frame_size, &m_IonDataArray[i], width, height, ConvertColorToGbmFormat(colorformat), ubwc_flags);
      if (buffer == NULL) {
        VLOGE("alloc input buffer failed");
        return false;
      }
      result = OMX_UseBuffer(m_Handle,
          &m_InputBufferHeaders[i],
          (OMX_U32) PORT_INDEX_IN,
          NULL,
          m_InputBufferSize,
          buffer);

    } else {
      OMX_S32 fds = 1;
      OMX_S32 ints = 2;
      buffer = (OMX_U8*)AllocVideoBuffer(p_inputmem, m_MetaFrameBufferSize, &m_IonDataArray[i], width, height, ConvertColorToGbmFormat(colorformat), ubwc_flags);
      if (buffer == NULL) {
        VLOGE("alloc input buffer failed");
        return false;
      }
      result = OMX_AllocateBuffer(m_Handle,
          &m_InputBufferHeaders[i],
          (OMX_U32) PORT_INDEX_IN,
          NULL,
          m_MetaFrameBufferSize);

      // GrallocSource
      private_handle_t * p_metahandle = new private_handle_t(p_inputmem->pmem_fd, m_MetaFrameBufferSize, 0, 1, colorformat, width, height);
      p_metahandle->base_metadata = (uint64_t)buffer;
      p_metahandle->fd_metadata = p_inputmem->pmem_fd;
      p_metahandle->unaligned_width = width;
      p_metahandle->unaligned_height = height;
      MetaBuffer *p_metabuffer = reinterpret_cast<MetaBuffer *>(m_InputBufferHeaders[i]->pBuffer);
      p_metabuffer->meta_handle = p_metahandle;
      p_metabuffer->buffer_type = GrallocSource;

      m_InputBufferHeaders[i]->pAppPrivate = p_inputmem;
      m_InputBufferHeaders[i]->pInputPortPrivate = buffer;
    }
    CHECK_RESULT("use input buffer failed", result);
  }

  // alloc advanced input buffer
  if (m_TestMode == MODE_PROFILE) {
    int32_t advanced_inputbuffer_size = GetFileSize();
    m_AdvancedInputBuffer = (unsigned char*)malloc(advanced_inputbuffer_size);

    if (m_AdvancedInputBuffer == NULL) {
      VLOGE("alloc advanced input buffer failed");
      return false;
    }
  }

  // alloc output buffer
  for (i = 0; i < m_OutputBufferCount; i++) {
    OMX_U8* p_buff;
    p_buff = (OMX_U8*)malloc(m_OutputBufferSize);
    result = OMX_UseBuffer(m_Handle,
        &m_OutputBufferHeaders[i],
        (OMX_U32) PORT_INDEX_OUT,
        NULL,
        m_OutputBufferSize,
        p_buff);
    CHECK_RESULT("use output buffer failed", result);
  }

  FUNCTION_EXIT();
  return true;
}

bool ReleaseBuffers(OMX_HANDLETYPE handle) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  int32_t i = 0;

  FUNCTION_ENTER();

  CHECK_HANDLE(handle);

  // release input buffer
  for (i = 0; i < m_InputBufferCount; i++) {
    if (m_InputBufferHeaders[i] != NULL) {
      if (m_MetadataMode) {
        if (((MetaBuffer *)(m_InputBufferHeaders[i]->pBuffer))->meta_handle) {
          free(((MetaBuffer *)(m_InputBufferHeaders[i]->pBuffer))->meta_handle);
          ((MetaBuffer *)(m_InputBufferHeaders[i]->pBuffer))->meta_handle = NULL;
        }
        FreeVideoBuffer((OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO*)m_InputBufferHeaders[i]->pAppPrivate,
            m_InputBufferHeaders[i]->pBuffer,
            m_MetaFrameBufferSize, &m_IonDataArray[i]);
      } else {
        FreeVideoBuffer((OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO*)m_InputBufferHeaders[i]->pAppPrivate,
            m_InputBufferHeaders[i]->pBuffer,
            m_InputBufferSize, &m_IonDataArray[i]);
      }

      delete (OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO*) m_InputBufferHeaders[i]->pAppPrivate;
      if (m_InputBufferHeaders[i]->pBuffer) {
        result = OMX_FreeBuffer(handle,
            PORT_INDEX_IN,
            m_InputBufferHeaders[i]);
      } else {
        VLOGE("buffer %d is null", i);
        result = OMX_ErrorUndefined;
      }
      CHECK_RESULT("free input buffer header failed", result);
    }
  }

  if (m_IonDataArray) {
    free(m_IonDataArray);
  }

  // release advanced input buffer
  if (m_TestMode == MODE_PROFILE && m_AdvancedInputBuffer != NULL) {
    free(m_AdvancedInputBuffer);
    m_AdvancedInputBuffer = NULL;
  }

  // release output buffer
  for (i = 0; i < m_OutputBufferCount; i++) {
    if (m_OutputBufferHeaders[i] != NULL) {
      if (m_OutputBufferHeaders[i]->pBuffer) {
        free(m_OutputBufferHeaders[i]->pBuffer);
      }
      result = OMX_FreeBuffer(handle,
          PORT_INDEX_OUT,
          m_OutputBufferHeaders[i]);
      CHECK_RESULT("free output buffer header failed", result);
    }
  }

  FUNCTION_EXIT();
  return true;
}

bool InitializeCodec(OMX_U32 codec) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_CALLBACKTYPE callbacks = { EventCallback,
    EmptyBufferDoneCallback,
    FillBufferDoneCallback};

  FUNCTION_ENTER();

  VLOGD("eCodec: %d", codec);

  if (codec >= OMX_VIDEO_CodingMax) {
    VLOGE("invalid parameter: eCodec: %d, %s", codec, ConvertEcodeToStr(codec));
    FUNCTION_EXIT();
    return false;
  }

  InitQueue();
  InitBufferManager();

  result = OMX_Init();
  CHECK_RESULT("OMX_Init failed:", result);

  m_Handle = NULL;
  switch (codec) {
#ifdef DISABLE_SW_VENC
    case OMX_VIDEO_CodingMPEG4:
      result = OMX_GetHandle(&m_Handle,
          (OMX_STRING)"OMX.qcom.video.encoder.mpeg4",
          NULL, &callbacks);
      CHECK_RESULT("get MPEG4 codec handle failed:", result);
      break;
    case OMX_VIDEO_CodingH263:
      result = OMX_GetHandle(&m_Handle,
          (OMX_STRING)"OMX.qcom.video.encoder.h263",
          NULL, &callbacks);
      CHECK_RESULT("get H263 codec handle failed:", result);
      break;
#else
    case OMX_VIDEO_CodingMPEG4:
      result = OMX_GetHandle(&m_Handle,
          (OMX_STRING)"OMX.qcom.video.encoder.mpeg4sw",
          NULL, &callbacks);
      CHECK_RESULT("get MPEG4 codec handle failed:", result);
      break;
    case OMX_VIDEO_CodingH263:
      result = OMX_GetHandle(&m_Handle,
          (OMX_STRING)"OMX.qcom.video.encoder.h263sw",
          NULL, &callbacks);
      CHECK_RESULT("get H263 codec handle failed:", result);
      break;
#endif
    case OMX_VIDEO_CodingVP8:
      result = OMX_GetHandle(&m_Handle,
          (OMX_STRING)"OMX.qcom.video.encoder.vp8",
          NULL, &callbacks);
      CHECK_RESULT("get VP8 codec handle failed:", result);
      break;
    case OMX_VIDEO_CodingHEVC:
      result = OMX_GetHandle(&m_Handle,
          (OMX_STRING)"OMX.qcom.video.encoder.hevc",
          NULL, &callbacks);
      CHECK_RESULT("get HEVC codec handle failed:", result);
      break;
    case OMX_VIDEO_CodingAVC:
      result = OMX_GetHandle(&m_Handle,
          (OMX_STRING)"OMX.qcom.video.encoder.avc",
          NULL, &callbacks);
      CHECK_RESULT("get AVC codec handle failed:", result);
      break;
    case OMX_VIDEO_CodingImageHEIC:
      result = OMX_GetHandle(&m_Handle,
          (OMX_STRING)"OMX.qcom.video.encoder.heic",
          NULL, &callbacks);
      CHECK_RESULT("get HEIC codec handle failed:", result);
      break;
    default:
      VLOGE("Unsupported eCodec:%d", codec);
      FUNCTION_EXIT();
      return false;
  }

  FUNCTION_EXIT();
  return true;
}

bool CropFrame(uint32_t height, uint32_t width) {
  if (height == 0 || width == 0) {
    VLOGD("crop skip");
    return true;
  }

  FUNCTION_ENTER();

  OMX_ERRORTYPE result = OMX_ErrorNone;
  QOMX_INDEXEXTRADATATYPE e;  // OMX_QcomIndexParamIndexExtraDataType
  OMX_INIT_STRUCT(&e, QOMX_INDEXEXTRADATATYPE);
  e.nPortIndex = (OMX_U32)PORT_INDEX_IN;
  e.nIndex = (OMX_INDEXTYPE)OMX_ExtraDataFrameDimension;
  e.bEnabled = OMX_TRUE;
  result = OMX_SetParameter(m_Handle, (OMX_INDEXTYPE)OMX_QcomIndexParamIndexExtraDataType, (OMX_PTR)&e);
  CHECK_RESULT("set FrameDimension failed:", result);

  FUNCTION_EXIT();
  return true;
}


bool ConfigEncoder(VideoCodecSetting_t *codec_settings) {
  bool ret = false;

  FUNCTION_ENTER();

  CHECK_HANDLE(m_Handle);

  if (codec_settings == NULL) {
    VLOGE("Invalid paramter");

    FUNCTION_EXIT();
    return false;
  }

  uint32_t scale_height = codec_settings->nScalingHeight;
  uint32_t scale_width = codec_settings->nScalingWidth;
  uint32_t crop_height = codec_settings->nRectangleRight - codec_settings->nRectangleLeft;
  uint32_t crop_width = codec_settings->nRectangleBottom - codec_settings->nRectangleTop;

  ret = SetFrameScale(scale_height, scale_width, crop_height, crop_width);
  CHECK_BOOL("set frame scale failed", ret);

  CropFrame(crop_height,crop_width);

  ret = SetInPortParameters(codec_settings->nFrameWidth,
      codec_settings->nFrameHeight,
      codec_settings->nFormat,
      codec_settings->nFrameRate);
  CHECK_BOOL("set inport failed", ret);
  codec_settings->nInputBufferSize = m_InputBufferSize;
  codec_settings->nInputBufferCount = m_InputBufferCount;
  VLOGD("Input buffer size: %d, count: %d", codec_settings->nInputBufferSize,
      codec_settings->nInputBufferCount);

  ret = SetOutPortParameters(codec_settings->nFrameWidth,
      codec_settings->nFrameHeight,
      codec_settings->nFrameRate,
      codec_settings->nBitRate,
      codec_settings->eCodec);
  CHECK_BOOL("set outport failed", ret);

  if (codec_settings->nUserProfile == 0) {
    OMX_U32 userProfile = 0;
    OMX_U32 eLevel = 0;
    ret = GetDefaultUserProfile((OMX_VIDEO_CODINGTYPE)codec_settings->eCodec,
        codec_settings->nFrameWidth,
        codec_settings->nFrameHeight,
        codec_settings->nFrameRate,
        codec_settings->nBitRate,
        &userProfile,
        &eLevel);
    CHECK_BOOL("get default userProfile and eLevel failed", ret);

    codec_settings->nUserProfile = (int32_t)userProfile;
    codec_settings->eLevel = (int32_t)eLevel;
  }

  switch (codec_settings->eCodec) {
    case OMX_VIDEO_CodingH263:
      ret = ConfigureH263Codec(codec_settings->nUserProfile,
          codec_settings->eLevel,
          codec_settings->nFrameRate);
      CHECK_BOOL("configure H263 codec failed", ret);
      break;
    case OMX_VIDEO_CodingMPEG4:
      ret = ConfigureMPEG4Codec(codec_settings->nUserProfile,
          codec_settings->eLevel,
          codec_settings->nFrameRate);
      CHECK_BOOL("configure MPEG4 codec failed", ret);
      break;
    case OMX_VIDEO_CodingAVC:
      ret = ConfigureAvcCodec(codec_settings->nUserProfile,
          codec_settings->eLevel,
          codec_settings->nFrameRate);
      CHECK_BOOL("configure AVC codec failed", ret);
      break;
    case OMX_VIDEO_CodingHEVC:
      ret = ConfigureHevcCodec(codec_settings->nUserProfile,
          codec_settings->eLevel,
          codec_settings->nFrameRate);
      CHECK_BOOL("configure HEVC codec failed", ret);
      break;
    case OMX_VIDEO_CodingVP8:
      ret = ConfigureVp8Codec(codec_settings->nUserProfile,
          codec_settings->eLevel,
          codec_settings->nFrameRate);
      CHECK_BOOL("configure VP8 codec failed", ret);
      break;
    case OMX_VIDEO_CodingImageHEIC:
      ret = ConfigureHEICImageCodec(codec_settings->nUserProfile,
          codec_settings->eLevel,
          codec_settings->nFrameRate);
      break;
    default:
      VLOGE("Invalid codec: %d", codec_settings->eCodec);
      FUNCTION_EXIT();
      return false;
  }

  ret = SetBitRateAndEControlRate(codec_settings->nBitRate,
      codec_settings->eControlRate);
  CHECK_BOOL("set bitrate and eControl rate failed", ret);

  if (codec_settings->eCodec == OMX_VIDEO_CodingHEVC ||
      codec_settings->eCodec == OMX_VIDEO_CodingAVC) {
    ret = EnablePrependSPSPPSToIDRFrame(
        codec_settings->bPrependSPSPPSToIDRFrame);
    CHECK_BOOL("set Prepend SPS/PPS failed", ret);
  }

  if (codec_settings->eCodec == OMX_VIDEO_CodingHEVC) {
    ret = SetIntraPeriod(codec_settings->nIDRPeriod,
        codec_settings->nPFrames,
        codec_settings->nBFrames);
    CHECK_BOOL("set intra period failed", ret);
  }

  if (codec_settings->eCodec == OMX_VIDEO_CodingAVC) {
    ret = SetAVCIntraPeriod(codec_settings->nIDRPeriod,
        codec_settings->nPFrames);
    CHECK_BOOL("set avc intra period failed", ret);
  }

  ret = ConfigErrorCorrection(codec_settings->eCodec,
      codec_settings->eResyncMarkerType,
      codec_settings->nResyncMarkerSpacing,
      codec_settings->nHECInterval);
  CHECK_BOOL("set Error correction failed", ret);

  ret = SetIntraRefresh(codec_settings->nFrameWidth,
      codec_settings->nFrameHeight,
      codec_settings->nRefreshMode,
      codec_settings->nIntraRefresh);
  CHECK_BOOL("set intral refresh failed", ret);

  /*if (codec_settings->eCodec == OMX_VIDEO_CodingAVC) {
    ret = SetFramePackingData(codec_settings->configFileName);
    CHECK_BOOL("set frame packing data failed", ret);
  }*/

  ret = SetFrameRate(codec_settings->nFrameRate);
  CHECK_BOOL("set frame rate failed", ret);

  ret = SetFrameMirror(codec_settings->nMirror);
  CHECK_BOOL("set frame mirror failed", ret);

  ret = SetFrameRotation(codec_settings->nRotation);
  CHECK_BOOL("set frame rotation failed", ret);

  // goto idle state, then register buffer
  ret = SetState(OMX_StateIdle, OMX_FALSE);
  CHECK_BOOL("set state to idle failed", ret);

  ret = RegisterBuffer(codec_settings->nFrameHeight, codec_settings->nFrameWidth,
      codec_settings->nFormat, codec_settings->eCodec);
  CHECK_BOOL("register buffer failed", ret);

  FUNCTION_EXIT();
  return ret;
}

bool PrepareForPerformanceTest() {
  FUNCTION_ENTER();
  bool result = true;
  int32_t frame_length = 0;
  while (result) {
    VLOGD("PrepareForPerformanceTest m_ReadFrameLen: %d", m_ReadFrameLen);
    frame_length = 0;
    result = GetNextFrameFromFile(m_AdvancedInputBuffer + m_InputBufferSize * m_ReadFramNum, &frame_length);
    if (result) {
      m_ReadFramNum++;
      m_ReadFrameLen = frame_length;
    }
    VLOGD("GetNextFrame: %d", m_ReadFramNum);
  }
  FUNCTION_EXIT();

  return 0;
}

bool GetNextFrameFromBuffer(unsigned char * p_buffer, int32_t *read_length, int32_t encode_frame_num) {
  FUNCTION_ENTER();
  VLOGD("GetNextFrameFromBuffer m_ReadFrameLen:%d, encodeFrameNum:%d", m_ReadFrameLen, encode_frame_num);
  if (m_ReadFramNum > encode_frame_num) {
    memscpy(p_buffer, sizeof(p_buffer), m_AdvancedInputBuffer + m_ReadFrameLen * encode_frame_num, m_ReadFrameLen);
    *read_length = m_ReadFrameLen;
  } else {
    *read_length = 0;
  }
  FUNCTION_EXIT();
  return true;
}

bool StartEncoding() {
  bool ret = false;
  OMX_ERRORTYPE result = OMX_ErrorNone;
  int32_t i = 0;

  FUNCTION_ENTER();

  CHECK_HANDLE(m_Handle);

  ret = SetState(OMX_StateExecuting, OMX_TRUE);
  CHECK_BOOL("set state to executing failed", ret);

  StartProcessThread();

  for (i = 0; i < m_OutputBufferCount; i++) {
    result = OMX_FillThisBuffer(m_Handle, m_OutputBufferHeaders[i]);
    CHECK_RESULT("request fill this buffer failed", result);
  }

  for (i = 0; i < m_InputBufferCount; i++) {
    BufferMessage_t buffer_msg;
    buffer_msg.p_buffer = m_InputBufferHeaders[i];
    PushInputEmptyBuffer(buffer_msg);
  }

  FUNCTION_EXIT();

  return ret;
}

bool StopEncoding() {
  bool ret = false;

  FUNCTION_ENTER();

  // notify process to exit
  m_InputThreadRunning = false;
  m_OutputThreadRunning = false;

  WaitProcessThreadExit();

  ret = SetState(OMX_StateIdle, OMX_FALSE);
  CHECK_BOOL("set state to idle failed", ret);

  FUNCTION_EXIT();
  return ret;
}

bool ReleaseCodec() {
  FUNCTION_ENTER();

  CHECK_HANDLE(m_Handle);

  bool ret = SetState(OMX_StateLoaded, OMX_FALSE);
  CHECK_BOOL("set state to loaded failed", ret);

  DestroyQueue();
  ReleaseBuffers(m_Handle);
  ReleaseBufferManager();

  ret = WaitState(OMX_StateLoaded);
  CHECK_BOOL("wait state to loaded failed", ret);

  if (m_Handle != NULL) {
    OMX_FreeHandle(m_Handle);
    m_Handle = NULL;
  }

  VLOGE("handle : %p", &m_Handle);
  VLOGE("Encode input frame: %d, output frame: %d",
      m_InputFrameNum, m_OutputFrameNum);

  FUNCTION_EXIT();

  return true;
}

int SetDynamicConfig(OMX_INDEXTYPE config_param, DynamicConfigData* config_data) {
  FUNCTION_ENTER();

  OMX_ERRORTYPE result = OMX_ErrorNone;
  if (config_param == OMX_IndexConfigVideoBitrate) {
    OMX_INIT_STRUCT(&config_data->bitrate, OMX_VIDEO_CONFIG_BITRATETYPE);
    config_data->bitrate.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
    result = OMX_SetConfig(m_Handle, config_param, (OMX_PTR) &config_data->bitrate);
  } else if (config_param == OMX_IndexConfigVideoFramerate) {
    OMX_INIT_STRUCT(&config_data->framerate, OMX_CONFIG_FRAMERATETYPE);
    config_data->framerate.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
    result = OMX_SetConfig(m_Handle, config_param, &config_data->framerate);
  }

  CHECK_RESULT("setDynamicConfig failed", result);
  FUNCTION_EXIT();
  return 0;
}

void PrintDynamicalData() {
  FUNCTION_ENTER();

  VLOGP("\n\n=======================Dynamical Data=====================");
  VLOGP("Encode Frame Time avg: %5dms", m_EncodeTotalTimeActal / m_OutputFrameNum);
  VLOGP("Encode Frame Time Min: %5dms", m_EncodeFrameTimeMin);
  VLOGP("Encode Frame Time Max: %5dms", m_EncodeFrameTimeMax);
  VLOGP("\n===========================================================\n\n");

  FUNCTION_EXIT();
}
