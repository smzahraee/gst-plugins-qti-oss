/*--------------------------------------------------------------------------
Copyright (c) 2010-2014, 2016-2018, The Linux Foundation. All rights reserved.

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
                    V E N C _ T E S T. C P P

DESCRIPTION

 This is the OMX test lite app .

REFERENCES

============================================================================*/

//usage
// FILE QVGA MP4 24 384000 100 enc_qvga.yuv QVGA_24.m4v
// FILE QCIF MP4 15 96000 0 foreman.qcif.yuv output_qcif.m4v
// FILE VGA MP4 24 1200000 218 enc_vga.yuv vga_output.m4v
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
//#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <string.h>
//#include <sys/stat.h>
#include "OMX_QCOMExtns.h"
#include "OMX_Core.h"

#define QTI_EXT 1

#include "OMX_Core.h"
#include "OMX_Video.h"
#include "OMX_VideoExt.h"
#include "OMX_IndexExt.h"
#include "OMX_Component.h"
#include "camera_test.h"
//#include "fb_test.h"
#include "venc_util.h"
#include "extra_data_handler.h"
#ifdef USE_ION
#include <linux/msm_ion.h>
#if TARGET_ION_ABI_VERSION >= 2
#include <ion/ion.h>
#include <linux/dma-buf.h>
#else
#include <linux/ion.h>
#endif
#endif
#include <media/msm_media_info.h>
#include "OMX_Types.h"
#ifdef USE_GBM
#include "gbm.h"
#include "gbm_priv.h"
#endif

typedef struct PrependSPSPPSToIDRFramesParams {
  OMX_U32 nSize;
  OMX_VERSIONTYPE nVersion;
  OMX_BOOL bEnable;
} PrependSPSPPSToIDRFramesParams;
#define DEADVALUE ((OMX_S32) 0xDEADDEAD) //in decimal : 3735936685
//////////////////////////
// MACROS
//////////////////////////

#define CHK(result) if ((result != OMX_ErrorNone) && (result != OMX_ErrorNoMore)) { E("*************** error *************"); exit(0); }
#define TEST_LOG
#ifdef VENC_SYSLOG
#include <cutils/log.h>
/// Debug message macro
#define D(fmt, ...) ALOGE("venc_test Debug %s::%d "fmt,              \
                         __FUNCTION__, __LINE__,                        \
                         ## __VA_ARGS__)

/// Error message macro
#define E(fmt, ...) ALOGE("venc_test Error %s::%d "fmt,            \
                         __FUNCTION__, __LINE__,                      \
                         ## __VA_ARGS__)

#else
#ifdef TEST_LOG
#define D(fmt, ...) fprintf(stderr, "venc_test Debug %s::%d " fmt "\n",   \
                            __FUNCTION__, __LINE__,                     \
                            ## __VA_ARGS__)

/// Error message macro
#define E(fmt, ...) fprintf(stderr, "venc_test Error %s::%d " fmt "\n", \
                            __FUNCTION__, __LINE__,                   \
                            ## __VA_ARGS__)
#else
#define D(fmt, ...)
#define E(fmt, ...)
#endif

#endif

//////////////////////////
// CONSTANTS
//////////////////////////
static const int MAX_MSG = 100;
//#warning do not hardcode these use port definition
static const int PORT_INDEX_IN = 0;
static const int PORT_INDEX_OUT = 1;

static const int NUM_IN_BUFFERS = 10;
static const int NUM_OUT_BUFFERS = 10;

//////////////////////////
/* MPEG4 profile and level table*/
static const unsigned int mpeg4_profile_level_table[][5]=
{
  /*max mb per frame, max mb per sec, max bitrate, level, profile*/
  {99,1485,64000,OMX_VIDEO_MPEG4Level0,OMX_VIDEO_MPEG4ProfileSimple},
  {99,1485,64000,OMX_VIDEO_MPEG4Level1,OMX_VIDEO_MPEG4ProfileSimple},
  {396,5940,128000,OMX_VIDEO_MPEG4Level2,OMX_VIDEO_MPEG4ProfileSimple},
  {396,11880,384000,OMX_VIDEO_MPEG4Level3,OMX_VIDEO_MPEG4ProfileSimple},
  {1200,36000,4000000,OMX_VIDEO_MPEG4Level4a,OMX_VIDEO_MPEG4ProfileSimple},
  {1620,40500,8000000,OMX_VIDEO_MPEG4Level5,OMX_VIDEO_MPEG4ProfileSimple},
  {3600,108000,12000000,OMX_VIDEO_MPEG4Level5,OMX_VIDEO_MPEG4ProfileSimple},
  {32400,972000,20000000,OMX_VIDEO_MPEG4Level5,OMX_VIDEO_MPEG4ProfileSimple},
  {34560,1036800,20000000,OMX_VIDEO_MPEG4Level5,OMX_VIDEO_MPEG4ProfileSimple},
  {0,0,0,0,0},

  {99,1485,128000,OMX_VIDEO_MPEG4Level0,OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {99,1485,128000,OMX_VIDEO_MPEG4Level1,OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {396,5940,384000,OMX_VIDEO_MPEG4Level2,OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {396,11880,768000,OMX_VIDEO_MPEG4Level3,OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {792,23760,3000000,OMX_VIDEO_MPEG4Level4,OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {1620,48600,8000000,OMX_VIDEO_MPEG4Level5,OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {32400,972000,20000000,OMX_VIDEO_MPEG4Level5,OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {34560,1036800,20000000,OMX_VIDEO_MPEG4Level5,OMX_VIDEO_MPEG4ProfileAdvancedSimple},
  {0,0,0,0,0},
};

/* H264 profile and level table*/
static const unsigned int h264_profile_level_table[][5]=
{
  /*max mb per frame, max mb per sec, max bitrate, level, profile*/
  {99,1485,64000,OMX_VIDEO_AVCLevel1,OMX_VIDEO_AVCProfileBaseline},
  {99,1485,128000,OMX_VIDEO_AVCLevel1b,OMX_VIDEO_AVCProfileBaseline},
  {396,3000,192000,OMX_VIDEO_AVCLevel11,OMX_VIDEO_AVCProfileBaseline},
  {396,6000,384000,OMX_VIDEO_AVCLevel12,OMX_VIDEO_AVCProfileBaseline},
  {396,11880,768000,OMX_VIDEO_AVCLevel13,OMX_VIDEO_AVCProfileBaseline},
  {396,11880,2000000,OMX_VIDEO_AVCLevel2,OMX_VIDEO_AVCProfileBaseline},
  {792,19800,4000000,OMX_VIDEO_AVCLevel21,OMX_VIDEO_AVCProfileBaseline},
  {1620,20250,4000000,OMX_VIDEO_AVCLevel22,OMX_VIDEO_AVCProfileBaseline},
  {1620,40500,10000000,OMX_VIDEO_AVCLevel3,OMX_VIDEO_AVCProfileBaseline},
  {3600,108000,14000000,OMX_VIDEO_AVCLevel31,OMX_VIDEO_AVCProfileBaseline},
  {5120,216000,20000000,OMX_VIDEO_AVCLevel32,OMX_VIDEO_AVCProfileBaseline},
  {8192,245760,20000000,OMX_VIDEO_AVCLevel4,OMX_VIDEO_AVCProfileBaseline},
  {8192,245760,50000000,OMX_VIDEO_AVCLevel41,OMX_VIDEO_AVCProfileBaseline},
  {8704,522240,50000000,OMX_VIDEO_AVCLevel42,OMX_VIDEO_AVCProfileBaseline},
  {22080,589824,135000000,OMX_VIDEO_AVCLevel5,OMX_VIDEO_AVCProfileBaseline},
  {36864,983040,240000000,OMX_VIDEO_AVCLevel51,OMX_VIDEO_AVCProfileBaseline},
  {36864,2073600,240000000,OMX_VIDEO_AVCLevel52,OMX_VIDEO_AVCProfileBaseline},
  {0,0,0,0,0},

  {99,1485,64000,OMX_VIDEO_AVCLevel1,OMX_VIDEO_AVCProfileMain},
  {99,1485,128000,OMX_VIDEO_AVCLevel1b,OMX_VIDEO_AVCProfileMain},
  {396,3000,192000,OMX_VIDEO_AVCLevel11,OMX_VIDEO_AVCProfileMain},
  {396,6000,384000,OMX_VIDEO_AVCLevel12,OMX_VIDEO_AVCProfileMain},
  {396,11880,768000,OMX_VIDEO_AVCLevel13,OMX_VIDEO_AVCProfileMain},
  {396,11880,2000000,OMX_VIDEO_AVCLevel2,OMX_VIDEO_AVCProfileMain},
  {792,19800,4000000,OMX_VIDEO_AVCLevel21,OMX_VIDEO_AVCProfileMain},
  {1620,20250,4000000,OMX_VIDEO_AVCLevel22,OMX_VIDEO_AVCProfileMain},
  {1620,40500,10000000,OMX_VIDEO_AVCLevel3,OMX_VIDEO_AVCProfileMain},
  {3600,108000,14000000,OMX_VIDEO_AVCLevel31,OMX_VIDEO_AVCProfileMain},
  {5120,216000,20000000,OMX_VIDEO_AVCLevel32,OMX_VIDEO_AVCProfileMain},
  {8192,245760,20000000,OMX_VIDEO_AVCLevel4,OMX_VIDEO_AVCProfileMain},
  {8192,245760,50000000,OMX_VIDEO_AVCLevel41,OMX_VIDEO_AVCProfileMain},
  {8704,522240,50000000,OMX_VIDEO_AVCLevel42,OMX_VIDEO_AVCProfileMain},
  {22080,589824,135000000,OMX_VIDEO_AVCLevel5,OMX_VIDEO_AVCProfileMain},
  {36864,983040,240000000,OMX_VIDEO_AVCLevel51,OMX_VIDEO_AVCProfileMain},
  {36864,2073600,240000000,OMX_VIDEO_AVCLevel52,OMX_VIDEO_AVCProfileMain},
  {0,0,0,0,0}

};

/* H263 profile and level table*/
static const unsigned int h263_profile_level_table[][5]=
{
  /*max mb per frame, max mb per sec, max bitrate, level, profile*/
  {99,1485,64000,OMX_VIDEO_H263Level10,OMX_VIDEO_H263ProfileBaseline},
  {396,5940,128000,OMX_VIDEO_H263Level20,OMX_VIDEO_H263ProfileBaseline},
  {396,11880,384000,OMX_VIDEO_H263Level30,OMX_VIDEO_H263ProfileBaseline},
  {396,11880,2048000,OMX_VIDEO_H263Level40,OMX_VIDEO_H263ProfileBaseline},
  {99,1485,128000,OMX_VIDEO_H263Level45,OMX_VIDEO_H263ProfileBaseline},
  {396,19800,4096000,OMX_VIDEO_H263Level50,OMX_VIDEO_H263ProfileBaseline},
  {810,40500,8192000,OMX_VIDEO_H263Level60,OMX_VIDEO_H263ProfileBaseline},
  {1620,81000,16384000,OMX_VIDEO_H263Level70,OMX_VIDEO_H263ProfileBaseline},
  {32400,972000,20000000,OMX_VIDEO_H263Level60,OMX_VIDEO_H263ProfileBaseline},
  {34560,1036800,20000000,OMX_VIDEO_H263Level70,OMX_VIDEO_H263ProfileBaseline},
  {0,0,0,0,0}
};
static const unsigned int VP8_profile_level_table[][5]=
{
  //TODO: !!!!! Legacy code, it's strange to use H263 profile and level for VP8. Need refine code.
  /*max mb per frame, max mb per sec, max bitrate, level, profile*/
  {99,1485,64000,OMX_VIDEO_H263Level10,OMX_VIDEO_H263ProfileBaseline},
  {396,5940,128000,OMX_VIDEO_H263Level20,OMX_VIDEO_H263ProfileBaseline},
  {396,11880,384000,OMX_VIDEO_H263Level30,OMX_VIDEO_H263ProfileBaseline},
  {396,11880,2048000,OMX_VIDEO_H263Level40,OMX_VIDEO_H263ProfileBaseline},
  {99,1485,128000,OMX_VIDEO_H263Level45,OMX_VIDEO_H263ProfileBaseline},
  {396,19800,4096000,OMX_VIDEO_H263Level50,OMX_VIDEO_H263ProfileBaseline},
  {810,40500,8192000,OMX_VIDEO_H263Level60,OMX_VIDEO_H263ProfileBaseline},
  {1620,81000,16384000,OMX_VIDEO_H263Level70,OMX_VIDEO_H263ProfileBaseline},
  {32400,972000,20000000,OMX_VIDEO_H263Level70,OMX_VIDEO_H263ProfileBaseline},
  {34560,1036800,20000000,OMX_VIDEO_H263Level70,OMX_VIDEO_H263ProfileBaseline},
  {0,0,0,0,0}
};

/* HEVC profile and level table*/
static const unsigned int hevc_profile_level_table[][5]= {
  /*max mb per frame, max mb per sec, max bitrate, level, profile*/
  /*36864 is max samples per frame, come from ITU-T H.265 table A.6 */
  /*552960 is max samples per sec, come from ITU-T H.265 table A.7 */
  {144/*=36864/256*/,2160/*=552960/256*/,128000,OMX_VIDEO_HEVCMainTierLevel1,OMX_VIDEO_HEVCProfileMain},
  {480/*=122880/256*/,14400/*=3686400/256*/,1500000,OMX_VIDEO_HEVCMainTierLevel2,OMX_VIDEO_HEVCProfileMain},
  {960/*=245760/256*/,28800/*=7372800/256*/,3000000,OMX_VIDEO_HEVCMainTierLevel21,OMX_VIDEO_HEVCProfileMain},
  {2160/*=552960/256*/,64800/*=16588800/256*/,6000000,OMX_VIDEO_HEVCMainTierLevel3,OMX_VIDEO_HEVCProfileMain},
  {3840/*=983040/256*/,129600/*=33177600/256*/,10000000,OMX_VIDEO_HEVCMainTierLevel31,OMX_VIDEO_HEVCProfileMain},
  {8704/*=2228224/256*/,261120/*=66846720/256*/,12000000,OMX_VIDEO_HEVCMainTierLevel4,OMX_VIDEO_HEVCProfileMain},
  {8704/*=2228224/256*/,522240/*=133693440/256*/,20000000,OMX_VIDEO_HEVCMainTierLevel41,OMX_VIDEO_HEVCProfileMain},
  {34816/*=8912896/256*/,1044480/*=267386880/256*/,25000000,OMX_VIDEO_HEVCMainTierLevel5,OMX_VIDEO_HEVCProfileMain},
  {34816/*=8912896/256*/,2088960/*=534773760/256*/,40000000,OMX_VIDEO_HEVCMainTierLevel51,OMX_VIDEO_HEVCProfileMain},
  {8704/*=2228224/256*/,522240/*=133693440/256*/,50000000,OMX_VIDEO_HEVCHighTierLevel41,OMX_VIDEO_HEVCProfileMain},
  {34816/*=8912896/256*/,1044480/*=267386880/256*/,100000000,OMX_VIDEO_HEVCHighTierLevel5,OMX_VIDEO_HEVCProfileMain},
  {34816/*=8912896/256*/,2088960/*=534773760/256*/,160000000,OMX_VIDEO_HEVCHighTierLevel51,OMX_VIDEO_HEVCProfileMain},
  {34816/*=8912896/256*/,4177920/*=1069547520/256*/,240000000,OMX_VIDEO_HEVCHighTierLevel52,OMX_VIDEO_HEVCProfileMain},
  {139264/*=35651584/256*/,4177920/*=1069547520/256*/,240000000,OMX_VIDEO_HEVCHighTierLevel6,OMX_VIDEO_HEVCProfileMain},
  {0,0,0,0,0},

  {144/*=36864/256*/,2160/*=552960/256*/,128000,OMX_VIDEO_HEVCMainTierLevel1,OMX_VIDEO_HEVCProfileMain10},
  {480/*=122880/256*/,14400/*=3686400/256*/,1500000,OMX_VIDEO_HEVCMainTierLevel2,OMX_VIDEO_HEVCProfileMain10},
  {960/*=245760/256*/,28800/*=7372800/256*/,3000000,OMX_VIDEO_HEVCMainTierLevel21,OMX_VIDEO_HEVCProfileMain10},
  {2160/*=552960/256*/,64800/*=16588800/256*/,6000000,OMX_VIDEO_HEVCMainTierLevel3,OMX_VIDEO_HEVCProfileMain10},
  {3840/*=983040/256*/,129600/*=33177600/256*/,10000000,OMX_VIDEO_HEVCMainTierLevel31,OMX_VIDEO_HEVCProfileMain10},
  {8704/*=2228224/256*/,261120/*=66846720/256*/,12000000,OMX_VIDEO_HEVCMainTierLevel4,OMX_VIDEO_HEVCProfileMain10},
  {8704/*=2228224/256*/,522240/*=133693440/256*/,20000000,OMX_VIDEO_HEVCMainTierLevel41,OMX_VIDEO_HEVCProfileMain10},
  {34816/*=8912896/256*/,1044480/*=267386880/256*/,25000000,OMX_VIDEO_HEVCMainTierLevel5,OMX_VIDEO_HEVCProfileMain10},
  {34816/*=8912896/256*/,2088960/*=534773760/256*/,40000000,OMX_VIDEO_HEVCMainTierLevel51,OMX_VIDEO_HEVCProfileMain10},
  {8704/*=2228224/256*/,522240/*=133693440/256*/,50000000,OMX_VIDEO_HEVCHighTierLevel41,OMX_VIDEO_HEVCProfileMain10},
  {34816/*=8912896/256*/,1044480/*=267386880/256*/,100000000,OMX_VIDEO_HEVCHighTierLevel5,OMX_VIDEO_HEVCProfileMain10},
  {34816/*=8912896/256*/,2088960/*=534773760/256*/,160000000,OMX_VIDEO_HEVCHighTierLevel51,OMX_VIDEO_HEVCProfileMain10},
  {34816/*=8912896/256*/,4177920/*=1069547520/256*/,240000000,OMX_VIDEO_HEVCHighTierLevel52,OMX_VIDEO_HEVCProfileMain10},
  {139264/*=35651584/256*/,4177920/*=1069547520/256*/,240000000,OMX_VIDEO_HEVCHighTierLevel6,OMX_VIDEO_HEVCProfileMain10},
  {0,0,0,0,0},
};

#define FloatToQ16(q, v) { (q) = (unsigned int) (65536*(double)(v)); }

//////////////////////////
// TYPES
//////////////////////////
struct ProfileType
{
  OMX_VIDEO_CODINGTYPE eCodec;
  OMX_VIDEO_MPEG4LEVELTYPE eLevel;
  OMX_VIDEO_CONTROLRATETYPE eControlRate;
  OMX_VIDEO_AVCSLICEMODETYPE eSliceMode;
  OMX_U32 nFrameWidth;
  OMX_U32 nFrameHeight;
  OMX_U32 nFrameBytes;
  OMX_U32 nFramestride;
  OMX_U32 nFrameScanlines;
  OMX_U32 nFrameRead;
  OMX_U32 nBitrate;
  float nFramerate;
  char* cInFileName;
  char* cOutFileName;
  OMX_U32 nUserProfile;
};

enum MsgId
{
  MSG_ID_OUTPUT_FRAME_DONE,
  MSG_ID_INPUT_FRAME_DONE,
  MSG_ID_MAX
};
union MsgData
{
  struct
  {
    OMX_BUFFERHEADERTYPE* pBuffer;
  } sBitstreamData;
};
struct Msg
{
  MsgId id;
  MsgData data;
};
struct MsgQ
{
  Msg q[MAX_MSG];
  int head;
  int size;
};

enum Mode
{
  MODE_PREVIEW,
  MODE_DISPLAY,
  MODE_PROFILE,
  MODE_FILE_ENCODE,
  MODE_LIVE_ENCODE
};

enum ResyncMarkerType
{
  RESYNC_MARKER_NONE,     ///< No resync marker
  RESYNC_MARKER_BYTE,     ///< BYTE Resync marker for MPEG4, H.264
  RESYNC_MARKER_MB,       ///< MB resync marker for MPEG4, H.264
  RESYNC_MARKER_GOB       ///< GOB resync marker for H.263
};

union DynamicConfigData
{
  OMX_VIDEO_CONFIG_BITRATETYPE bitrate;
  OMX_CONFIG_FRAMERATETYPE framerate;
  QOMX_VIDEO_INTRAPERIODTYPE intraperiod;
  OMX_CONFIG_INTRAREFRESHVOPTYPE intravoprefresh;
  OMX_CONFIG_ROTATIONTYPE rotation;
  float f_framerate;
};

struct DynamicConfig
{
  bool pending;
  unsigned frame_num;
  OMX_INDEXTYPE config_param;
  union DynamicConfigData config_data;
};

#ifdef USE_ION
struct enc_ion
{
  int ion_device_fd;
  struct ion_allocation_data alloc_data;
  int data_fd;
#ifdef USE_GBM
  struct gbm_device *gbm;
  struct gbm_bo *bo;
  int meta_fd;
#endif
};
#endif
struct StoreMetaDataInBuffersParams {
  OMX_U32 nSize;
  OMX_VERSIONTYPE nVersion;
  OMX_U32 nPortIndex;
  OMX_BOOL bStoreMetaData;
};
typedef enum _MetaBufferType {
#ifdef USE_NATIVE_HANDLE_SOURCE
  CameraSource = 3,
#else
  CameraSource = 0,
#endif
  GrallocSource = 1,
}MetaBufferType;
#define ITUR601 0x200000
typedef struct _NativeHandle {
  OMX_S32 version;        /* sizeof(native_handle_t) */
  OMX_S32 numFds;         /* number of file-descriptors at &data[0] */
  OMX_S32 numInts;        /* number of ints at &data[numFds] */
  OMX_S32 data[0];        /* numFds + numInts ints */
}NativeHandle;
typedef struct _MetaBuffer {
  MetaBufferType buffer_type;
  NativeHandle* meta_handle;
}MetaBuffer;
#define OMX_SPEC_VERSION 0x00000101
#define OMX_INIT_STRUCT(_s_, _name_)            \
  memset((_s_), 0x0, sizeof(_name_));          \
  (_s_)->nSize = sizeof(_name_);               \
  (_s_)->nVersion.nVersion = OMX_SPEC_VERSION

//////////////////////////
// MODULE VARS
//////////////////////////
static pthread_mutex_t m_mutex;
static pthread_cond_t m_signal;
static MsgQ m_sMsgQ;

//#warning determine how many buffers we really have
OMX_STATETYPE m_eState = OMX_StateInvalid;
OMX_COMPONENTTYPE m_sComponent;
OMX_HANDLETYPE m_hHandle = NULL;
OMX_BUFFERHEADERTYPE* m_pOutBuffers[NUM_OUT_BUFFERS] = {NULL};
OMX_BUFFERHEADERTYPE* m_pInBuffers[NUM_IN_BUFFERS] = {NULL};
OMX_BOOL m_bInFrameFree[NUM_IN_BUFFERS];
unsigned int m_num_in_buffers = 0;
unsigned int m_num_out_buffers = 0;

int m_device_fd = -1;
#ifdef USE_GBM
struct gbm_device *m_gbm_device = NULL;
#endif

#ifdef USE_ION
struct enc_ion* m_ion_data_array = NULL;
#endif

ProfileType m_sProfile;

static int m_nFramePlay = 0;
static int m_rotation = 0;
static int m_eMode = MODE_PREVIEW;
static int m_nInFd = -1;
static FILE * m_nOutFd;
static int m_nTimeStamp = 0;
static int m_nFrameIn = 0; // frames pushed to encoder
static int m_nFrameOut = 0; // frames returned by encoder
static int m_scaling_width = 0;
static int m_scaling_height = 0;
static int m_nAVCSliceMode = 0;
static bool m_bWatchDogKicked = false;
static int m_eMetaMode = 0;
FILE  *m_pDynConfFile = NULL;
static struct DynamicConfig m_dynamic_config;

/* Statistics Logging */
static long long m_tot_bufsize = 0;
int m_ebd_cnt=0, m_fbd_cnt=0;

#ifdef USE_ION
#ifdef USE_GBM
static const char* PMEM_DEVICE = "/dev/dri/renderD128";
#else
static const char* PMEM_DEVICE = "/dev/ion";
#endif
#elif defined MAX_RES_720P
static const char* PMEM_DEVICE = "/dev/pmem_adsp";
#elif defined MAX_RES_1080P_EBI
static const char* PMEM_DEVICE  = "/dev/pmem_adsp";
#elif defined MAX_RES_1080P
static const char* PMEM_DEVICE = "/dev/pmem_smipool";
#else
#error PMEM_DEVICE cannot be determined.
#endif


//////////////////////////
// MODULE FUNCTIONS
//////////////////////////
OMX_ERRORTYPE GetVendorExtension(char* sExtensionName, OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE *ext) {
    OMX_ERRORTYPE result = OMX_ErrorNone;

  if(!ext) {
    E("\nNULL vendor extension instance");
    return OMX_ErrorUndefined;
  }

  //check all supported vendor extensions and get the index for the expected one
  for(OMX_U32 index = 0;;index++) {
    ext->nIndex = index;
    result = OMX_GetConfig(m_hHandle,
      (OMX_INDEXTYPE)OMX_IndexConfigAndroidVendorExtension,
      (OMX_PTR)ext);

    if(result == OMX_ErrorNone) {
      if(!strcmp((char*)ext->cName, sExtensionName)) {
        D("find extension %s\n", sExtensionName);
        return result;
      }
    }
    else {
      E("\nfailed to get vendor extension %s", sExtensionName);
      return OMX_ErrorUndefined;
    }
  }
  return OMX_ErrorUndefined;
}


OMX_ERRORTYPE SetVendorRateControlMode(OMX_VIDEO_CONTROLRATETYPE eControlRate) {
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE* ext = NULL;
  char extensionName[] = "qti-ext-enc-bitrate-mode";
  OMX_U32 paramSizeUsed = 1;
  OMX_U32 size = sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE) +
              (paramSizeUsed - 1) * sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE::nParam);

  ext = (OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE*)malloc(size);
  if(!ext) {
    E("\nUnable to create rate control vendor extension instance");
    return OMX_ErrorUndefined;
  }

  memset(ext, 0, sizeof(OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE));
  ext->nSize = size;
  ext->nParamSizeUsed = paramSizeUsed;
  result = GetVendorExtension(extensionName, ext);

  if(result == OMX_ErrorNone) {
    D("set rate control vendor extension");
    ext->nParam[0].bSet = OMX_TRUE;
    ext->nParam[0].nInt32 = eControlRate;
    result = OMX_SetConfig(m_hHandle,
            (OMX_INDEXTYPE)OMX_IndexConfigAndroidVendorExtension,
            (OMX_PTR)ext);
  }

  if(result != OMX_ErrorNone) {
    E("\nFailed to SetConfig - OMX_CONFIG_ANDROID_VENDOR_EXTENSIONTYPE");
  }

  if(ext) {
    free(ext);
    ext = NULL;
  }
  return result;
}

void* PmemMalloc(OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO* pMem, int nSize, struct enc_ion *ion_data_ptr)
{
  void *pvirt = NULL;
  int rc = 0;

  if (!pMem || !ion_data_ptr)
    return NULL;

#ifdef USE_ION
#ifdef USE_GBM
  struct gbm_bo *bo = NULL;
  int bo_fd = -1, meta_fd = -1;
  int size = nSize;
  size = (size + 4096 - 1) & ~(4096 - 1);

  D("use gbm\n");
  ion_data_ptr->ion_device_fd = m_device_fd;
  if(ion_data_ptr->ion_device_fd < 0)
  {
    E("\nERROR: gbm Device open() Failed");
    goto error_handle;
  }
  ion_data_ptr->gbm = m_gbm_device;
  if (ion_data_ptr->gbm == NULL)
  {
    E("gbm_create_device failed\n");
    goto error_handle;
  }
  bo = gbm_bo_create(ion_data_ptr->gbm, m_sProfile.nFrameWidth, m_sProfile.nFrameHeight, GBM_FORMAT_NV12, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  if(bo == NULL) {
    E("Create bo failed \n");
    goto error_handle;
  }
  ion_data_ptr->bo = bo;
  bo_fd = gbm_bo_get_fd(bo);
  if(bo_fd < 0) {
    E("Get bo fd failed \n");
    goto error_handle;
  }
  ion_data_ptr->data_fd = bo_fd;
  gbm_perform(GBM_PERFORM_GET_METADATA_ION_FD, bo, &meta_fd);
  if(meta_fd < 0) {
    E("Get bo meta fd failed \n");
    goto error_handle;
  }
  ion_data_ptr->meta_fd = meta_fd;
  D("gbm buffer size: app calculate size %d, gbm internal calculate size %d\n", size, bo->size);
  if (size != bo->size) {
    E("\n!!!!! app calculated size isn't equal to gbm bo internal calculated size !!!!\n");
    CHK(OMX_ErrorUndefined);
  }
#else
  ion_data_ptr->ion_device_fd = m_device_fd;
  if(ion_data_ptr->ion_device_fd < 0)
  {
    E("\nERROR: ION Device open() Failed");
    goto error_handle;
  }
  nSize = (nSize + 4095) & (~4095);
  ion_data_ptr->alloc_data.len = nSize;
  ion_data_ptr->alloc_data.heap_id_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);
  ion_data_ptr->alloc_data.flags = 0;

  rc = ion_alloc_fd(ion_data_ptr->ion_device_fd, ion_data_ptr->alloc_data.len, 0,
                      ion_data_ptr->alloc_data.heap_id_mask, ion_data_ptr->alloc_data.flags,
                      &ion_data_ptr->data_fd);
#endif
  pMem->pmem_fd = ion_data_ptr->data_fd;
#else
  pMem->pmem_fd = m_device_fd;
  if ((int)(pMem->pmem_fd) < 0)
    return NULL;
  nSize = (nSize + 4095) & (~4095);
#endif
  pMem->offset = 0;
  pvirt = mmap(NULL, nSize,
                PROT_READ | PROT_WRITE,
                MAP_SHARED, pMem->pmem_fd, pMem->offset);
  if (pvirt == (void*) MAP_FAILED)
  {
    goto error_handle;
  }
  D("allocated pMem->fd = %lu pvirt=%p, pMem->phys(offset)=0x%lx, size = %d", pMem->pmem_fd,
       pvirt, pMem->offset, nSize);

  //Clean total frame memory content. For some non-MB-aligned encoding, like 1920x1080,
  //the extra 1081~1088 line's content still probably affect encoded result if VPU don't do special operation for those extra line.
  //Therefore, it's better to clean those extra lines in advance.
  D("Clean frame buffer's total content (size %d) as 0\n", nSize);
  memset(pvirt, 0, nSize);

  return pvirt;
error_handle:
#ifdef USE_ION
#ifdef USE_GBM
    if (ion_data_ptr->bo)
      gbm_bo_destroy(ion_data_ptr->bo);
    ion_data_ptr->bo = NULL;
    ion_data_ptr->meta_fd = -1;
#else
    close(ion_data_ptr->data_fd);
    ion_data_ptr->data_fd =-1;
#endif
#endif
  return NULL;
}

int PmemFree(OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO* pMem, void* pvirt, int nSize, struct enc_ion *ion_data_ptr)
{
  if (!pMem || !pvirt || !ion_data_ptr)
    return -1;

  nSize = (nSize + 4095) & (~4095);
  munmap(pvirt, nSize);
#ifdef USE_ION
#ifdef USE_GBM
  if (ion_data_ptr->bo)
    gbm_bo_destroy(ion_data_ptr->bo);
  ion_data_ptr->bo = NULL;
  ion_data_ptr->meta_fd = -1;
#else
  close(ion_data_ptr->data_fd);
  ion_data_ptr->data_fd =-1;
#endif
#endif
  return 0;
}
void PrintFramePackArrangement(OMX_QCOM_FRAME_PACK_ARRANGEMENT framePackingArrangement)
{
  printf("id (%lu)\n",
         framePackingArrangement.id);
  printf("cancel_flag (%lu)\n",
         framePackingArrangement.cancel_flag);
  printf("type (%lu)\n",
         framePackingArrangement.type);
  printf("quincunx_sampling_flag (%lu)\n",
         framePackingArrangement.quincunx_sampling_flag);
  printf("content_interpretation_type (%lu)\n",
         framePackingArrangement.content_interpretation_type);
  printf("spatial_flipping_flag (%lu)\n",
         framePackingArrangement.spatial_flipping_flag);
  printf("frame0_flipped_flag (%lu)\n",
         framePackingArrangement.frame0_flipped_flag);
  printf("field_views_flag (%lu)\n",
         framePackingArrangement.field_views_flag);
  printf("current_frame_is_frame0_flag (%lu)\n",
         framePackingArrangement.current_frame_is_frame0_flag);
  printf("frame0_self_contained_flag (%lu)\n",
         framePackingArrangement.frame0_self_contained_flag);
  printf("frame1_self_contained_flag (%lu)\n",
         framePackingArrangement.frame1_self_contained_flag);
  printf("frame0_grid_position_x (%lu)\n",
         framePackingArrangement.frame0_grid_position_x);
  printf("frame0_grid_position_y (%lu)\n",
         framePackingArrangement.frame0_grid_position_y);
  printf("frame1_grid_position_x (%lu)\n",
         framePackingArrangement.frame1_grid_position_x);
  printf("frame1_grid_position_y (%lu)\n",
         framePackingArrangement.frame1_grid_position_y);
  printf("reserved_byte (%lu)\n",
         framePackingArrangement.reserved_byte);
  printf("repetition_period (%lu)\n",
         framePackingArrangement.repetition_period);
  printf("extension_flag (%lu)\n",
         framePackingArrangement.extension_flag);
}
void SetState(OMX_STATETYPE eState)
{
#define GOTO_STATE(eState)                      \
  case eState:                                 \
  {                                         \
    D("Going to state " # eState"...");            \
    OMX_SendCommand(m_hHandle,                     \
                    OMX_CommandStateSet,           \
                    (OMX_U32) eState,              \
                    NULL);                         \
    while (m_eState != eState)                     \
    {                                              \
      sleep(1);                               \
    }                                              \
    D("Now in state " # eState);                   \
    break;                                         \
  }

  switch (eState)
  {
    GOTO_STATE(OMX_StateLoaded);
    GOTO_STATE(OMX_StateIdle);
    GOTO_STATE(OMX_StateExecuting);
    GOTO_STATE(OMX_StateInvalid);
    GOTO_STATE(OMX_StateWaitForResources);
    GOTO_STATE(OMX_StatePause);
  }
}
////////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE ConfigureEncoder()
{
  OMX_ERRORTYPE result = OMX_ErrorNone;
  unsigned const int *profile_tbl = (unsigned int const *)mpeg4_profile_level_table;
  OMX_U32 mb_per_sec, mb_per_frame;
  bool profile_level_found = false;
  OMX_U32 eProfile,eLevel;

  OMX_PARAM_PORTDEFINITIONTYPE portdef; // OMX_IndexParamPortDefinition
#ifdef QTI_EXT
  OMX_QCOM_PARAM_PORTDEFINITIONTYPE qPortDefnType;
#endif
  portdef.nPortIndex = (OMX_U32) 0; // input
  result = OMX_GetParameter(m_hHandle,
                            OMX_IndexParamPortDefinition,
                            &portdef);
  E("\n OMX_IndexParamPortDefinition Get Paramter on input port");
  CHK(result);
  portdef.format.video.nFrameWidth = m_sProfile.nFrameWidth;
  portdef.format.video.nFrameHeight = m_sProfile.nFrameHeight;
  portdef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE) QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m;

  E ("\n Height %lu width %lu bit rate %lu",portdef.format.video.nFrameHeight
     ,portdef.format.video.nFrameWidth,portdef.format.video.nBitrate);
  result = OMX_SetParameter(m_hHandle,
                            OMX_IndexParamPortDefinition,
                            &portdef);
  E("\n OMX_IndexParamPortDefinition Set Paramter on input port");
  CHK(result);
  // once more to get proper buffer size
  result = OMX_GetParameter(m_hHandle,
                            OMX_IndexParamPortDefinition,
                            &portdef);
  E("\n OMX_IndexParamPortDefinition Get Paramter on input port, 2nd pass");
  CHK(result);
  // update size accordingly
  D("\n !!!!! input port OMX calculated size is %d, nFrameBytes %d", portdef.nBufferSize, m_sProfile.nFrameBytes);
  if (m_sProfile.nFrameBytes != portdef.nBufferSize) {
    E("!!! app calculated nFrameBytes %d isn't equal to OMX calculated size %d\n", m_sProfile.nFrameBytes, portdef.nBufferSize);
    CHK(OMX_ErrorUndefined);
  }
  m_sProfile.nFrameBytes = portdef.nBufferSize;
  portdef.nPortIndex = (OMX_U32) 1; // output
  result = OMX_GetParameter(m_hHandle,
                            OMX_IndexParamPortDefinition,
                            &portdef);
  E("\n OMX_IndexParamPortDefinition Get Paramter on output port");
  CHK(result);

  if (m_scaling_width == 0 || m_scaling_height == 0) {
    portdef.format.video.nFrameWidth = m_sProfile.nFrameWidth;
    portdef.format.video.nFrameHeight = m_sProfile.nFrameHeight;
  }
  else {
    portdef.format.video.nFrameWidth = m_scaling_width;
    portdef.format.video.nFrameHeight = m_scaling_height;
  }

  portdef.format.video.nBitrate = m_sProfile.nBitrate;
  FloatToQ16(portdef.format.video.xFramerate, m_sProfile.nFramerate);//nFramerate is float
  result = OMX_SetParameter(m_hHandle,
                            OMX_IndexParamPortDefinition,
                            &portdef);
  E("\n OMX_IndexParamPortDefinition Set Paramter on output port");
  CHK(result);

#ifdef QTI_EXT

qPortDefnType.nPortIndex = PORT_INDEX_IN;
qPortDefnType.nMemRegion = OMX_QCOM_MemRegionEBI1;
qPortDefnType.nSize = sizeof(OMX_QCOM_PARAM_PORTDEFINITIONTYPE);

result = OMX_SetParameter(m_hHandle,
                             (OMX_INDEXTYPE)OMX_QcomIndexPortDefn,
                             &qPortDefnType);

#endif
  if (!m_sProfile.nUserProfile) // profile not set by user, go ahead with table calculation
  {
    //validate the ht,width,fps,bitrate and set the appropriate profile and level
    if(m_sProfile.eCodec == OMX_VIDEO_CodingMPEG4)
    {
      profile_tbl = (unsigned int const *)mpeg4_profile_level_table;
    }
    else if(m_sProfile.eCodec == OMX_VIDEO_CodingAVC)
    {
      profile_tbl = (unsigned int const *)h264_profile_level_table;
    }
    else if(m_sProfile.eCodec == OMX_VIDEO_CodingH263)
    {
      profile_tbl = (unsigned int const *)h263_profile_level_table;
    }
    else if(m_sProfile.eCodec == OMX_VIDEO_CodingVP8)
    {
      profile_tbl = (unsigned int const *)VP8_profile_level_table;
    }
    else if(m_sProfile.eCodec == OMX_VIDEO_CodingHEVC)
    {
      profile_tbl = (unsigned int const *)hevc_profile_level_table;
    }
    mb_per_frame = ((m_sProfile.nFrameHeight+15)>>4)*
                 ((m_sProfile.nFrameWidth+15)>>4);

    mb_per_sec = mb_per_frame*(m_sProfile.nFramerate);

    printf("mb_per_frame: %d, mb_per_sec: %d, m_sProfile.nBitrate: %d\n", mb_per_frame, mb_per_sec, m_sProfile.nBitrate);

    do{
      if(mb_per_frame <= (unsigned int)profile_tbl[0])
      {
        if(mb_per_sec <= (unsigned int)profile_tbl[1])
        {
          if(m_sProfile.nBitrate <= (unsigned int)profile_tbl[2])
          {
            eLevel = (int)profile_tbl[3];
            eProfile = (int)profile_tbl[4];
            E("\n profile/level found: %lu/%lu\n",eProfile, eLevel);
            profile_level_found = true;
            break;
          }
        }
      }
      profile_tbl = profile_tbl + 5;
    }while(profile_tbl[0] != 0);

    if ( profile_level_found != true )
    {
      E("\n Error: Unsupported profile/level\n");
      return OMX_ErrorNone;
    }
  }
  else // Profile set by user!
  {
    eProfile = m_sProfile.nUserProfile;
    eLevel = 0;
  }
  if (m_sProfile.eCodec == OMX_VIDEO_CodingH263)
  {
    D("Configuring H263...");

    OMX_VIDEO_PARAM_H263TYPE h263;
    result = OMX_GetParameter(m_hHandle,
                              OMX_IndexParamVideoH263,
                              &h263);
    CHK(result);
    h263.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
    h263.nPFrames = m_sProfile.nFramerate * 2 - 1; // intra period
    h263.nBFrames = 0;
    h263.eProfile = (OMX_VIDEO_H263PROFILETYPE)eProfile;
    h263.eLevel = (OMX_VIDEO_H263LEVELTYPE)eLevel;
    h263.bPLUSPTYPEAllowed = OMX_FALSE;
    h263.nAllowedPictureTypes = 2;
    h263.bForceRoundingTypeToZero = OMX_TRUE;
    h263.nPictureHeaderRepetition = 0;
    h263.nGOBHeaderInterval = 1;
    result = OMX_SetParameter(m_hHandle,
                              OMX_IndexParamVideoH263,
                              &h263);
  }
  else
  {
    D("Configuring MP4/H264...");

    OMX_VIDEO_PARAM_PROFILELEVELTYPE profileLevel; // OMX_IndexParamVideoProfileLevelCurrent
    profileLevel.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
    profileLevel.eProfile = eProfile;
    profileLevel.eLevel =  eLevel;
    result = OMX_SetParameter(m_hHandle,
                              OMX_IndexParamVideoProfileLevelCurrent,
                              &profileLevel);
    E("\n OMX_IndexParamVideoProfileLevelCurrent Set Paramter port");
    CHK(result);
    //profileLevel.eLevel = (OMX_U32) m_sProfile.eLevel;
    result = OMX_GetParameter(m_hHandle,
                              OMX_IndexParamVideoProfileLevelCurrent,
                              &profileLevel);
    E("\n OMX_IndexParamVideoProfileLevelCurrent Get Paramter port");
    D ("\n Profile = %lu level = %lu",profileLevel.eProfile,profileLevel.eLevel);
    CHK(result);

    if (m_sProfile.eCodec == OMX_VIDEO_CodingMPEG4)
    {
      OMX_VIDEO_PARAM_MPEG4TYPE mp4; // OMX_IndexParamVideoMpeg4
      result = OMX_GetParameter(m_hHandle,
                                OMX_IndexParamVideoMpeg4,
                                &mp4);
      CHK(result);
      mp4.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
      mp4.nTimeIncRes = 1000;
      result = OMX_SetParameter(m_hHandle,
                                OMX_IndexParamVideoMpeg4,
                                &mp4);
      CHK(result);
    }
  }

  if (m_sProfile.eCodec == OMX_VIDEO_CodingAVC)
  {
/////////////C A B A C ///A N D/////D E B L O C K I N G /////////////////
    OMX_VIDEO_PARAM_AVCTYPE avcdata;
    avcdata.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
    result = OMX_GetParameter(m_hHandle,
                              OMX_IndexParamVideoAvc,
                              &avcdata);
    CHK(result);
    avcdata.nPFrames = 6;
    avcdata.nBFrames = 0;
    avcdata.bUseHadamard = OMX_FALSE;
    avcdata.nRefFrames = 1;
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
    avcdata.nPFrames = m_sProfile.nFramerate * 2 - 1;
    avcdata.nBFrames = 0;
    // TEST VALUES (CHANGE FOR DIFF CONFIG's)
 //      avcdata.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;
    avcdata.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterDisable;
    //   avcdata.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterDisableSliceBoundary;
//      avcdata.bEntropyCodingCABAC = OMX_FALSE;
       //avcdata.bEntropyCodingCABAC = OMX_TRUE;
//       avcdata.nCabacInitIdc = 1;
///////////////////////////////////////////////

    result = OMX_SetParameter(m_hHandle,
                             OMX_IndexParamVideoAvc,
                             &avcdata);
    CHK(result);
/////////////C A B A C ///A N D/////D E B L O C K I N G /////////////////
  }
  /////////////////////////bitrate/////////////////////
  if (m_sProfile.eControlRate == OMX_Video_ControlRateDisable) {
    D("Setting vendor extended rate control mode RC_OFF");
    result = SetVendorRateControlMode(m_sProfile.eControlRate);
  }
  else if (m_sProfile.nBitrate != DEADVALUE &&
           (m_sProfile.eControlRate == OMX_Video_ControlRateVariableSkipFrames ||
            m_sProfile.eControlRate == OMX_Video_ControlRateConstantSkipFrames)) {
    D("Setting vendor extended rate control mode controlrate skip");
    result = SetVendorRateControlMode(m_sProfile.eControlRate);
    CHK(result);
    OMX_VIDEO_PARAM_BITRATETYPE bitrate;
    memset(&bitrate, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
    bitrate.nSize = sizeof(OMX_VIDEO_PARAM_BITRATETYPE);
    bitrate.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
    result = OMX_GetParameter(m_hHandle,
          OMX_IndexParamVideoBitrate, (OMX_PTR)&bitrate);
    if (result != OMX_ErrorNone) {
      return result;
    }
    //no need eControlRate
    bitrate.nTargetBitrate = m_sProfile.nBitrate;
    bitrate.nSize = sizeof(OMX_VIDEO_PARAM_BITRATETYPE);
    result = OMX_SetParameter(m_hHandle,
          OMX_IndexParamVideoBitrate, (OMX_PTR)&bitrate);
  }
  else if (m_sProfile.nBitrate != DEADVALUE) {
    D("Setting common rate control mode eControlRate %d bits %d\n",m_sProfile.eControlRate,m_sProfile.nBitrate);
    OMX_VIDEO_PARAM_BITRATETYPE bitrate;

    memset(&bitrate, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
    bitrate.nSize = sizeof(OMX_VIDEO_PARAM_BITRATETYPE);
    bitrate.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
    result = OMX_GetParameter(m_hHandle,
          OMX_IndexParamVideoBitrate, (OMX_PTR)&bitrate);
    if (result != OMX_ErrorNone) {
      return result;
    }
    bitrate.eControlRate = m_sProfile.eControlRate;//ControlRate
    bitrate.nTargetBitrate = m_sProfile.nBitrate;
    bitrate.nSize = sizeof(OMX_VIDEO_PARAM_BITRATETYPE);
    result = OMX_SetParameter(m_hHandle,
        OMX_IndexParamVideoBitrate, (OMX_PTR)&bitrate);

  }
  /////////////////////////bitrate////////////////////////////////
  E("\n OMX_IndexParamVideoBitrate Set Paramter port");
  CHK(result);
  ///////////////////////////////////////
  // set SPS/PPS insertion for IDR frames
  ///////////////////////////////////////
  PrependSPSPPSToIDRFramesParams param;
  memset(&param, 0, sizeof(PrependSPSPPSToIDRFramesParams));
  param.nSize = sizeof(PrependSPSPPSToIDRFramesParams);
  param.bEnable = OMX_FALSE;
  D ("\n Set SPS/PPS headers: %d", param.bEnable);
  result = OMX_SetParameter(m_hHandle,
           (OMX_INDEXTYPE)OMX_QcomIndexParamSequenceHeaderWithIDR,
           (OMX_PTR)&param);

  if (result != OMX_ErrorNone) {
    return result;
  }
  ///////////////////////////////////////
  // set AU delimiters for video stream
  ///////////////////////////////////////
  OMX_QCOM_VIDEO_CONFIG_AUD param_aud;
  memset(&param_aud, 0, sizeof(OMX_QCOM_VIDEO_CONFIG_AUD));
  param_aud.nSize = sizeof(OMX_QCOM_VIDEO_CONFIG_AUD);
  param_aud.bEnable = OMX_FALSE;
  D ("\n et AU Delimiters = %d",param_aud.bEnable);
  result = OMX_SetParameter(m_hHandle,
           (OMX_INDEXTYPE)OMX_QcomIndexParamAUDelimiter,
           (OMX_PTR)&param_aud);
  CHK(result);
///////////////////I N T R A P E R I O D ///////////////////

  QOMX_VIDEO_INTRAPERIODTYPE intra;

  intra.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
  result = OMX_GetConfig(m_hHandle,
                      (OMX_INDEXTYPE) QOMX_IndexConfigVideoIntraperiod,
                      (OMX_PTR) &intra);

  if (result == OMX_ErrorNone)
  {
    intra.nPFrames = (OMX_U32) (2 * m_sProfile.nFramerate - 1); //setting I
                                                                //frame interval to
                                                                //2 x framerate
    intra.nIDRPeriod = 1; //every I frame is an IDR
    intra.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
    result = OMX_SetConfig(m_hHandle,
                           (OMX_INDEXTYPE) QOMX_IndexConfigVideoIntraperiod,
                           (OMX_PTR) &intra);
  }
  else
  {
    E("failed to get state", 0, 0, 0);
  }


///////////////////I N T R A P E R I O D ///////////////////


///////////////////E R R O R C O R R E C T I O N ///////////////////

  ResyncMarkerType eResyncMarkerType = RESYNC_MARKER_NONE;
  unsigned long int nResyncMarkerSpacing = 0;
  OMX_BOOL enableHEC = OMX_FALSE;

//For Testing ONLY
  if (m_sProfile.eCodec == OMX_VIDEO_CodingMPEG4)
  {
// MPEG4
//      eResyncMarkerType = RESYNC_MARKER_BYTE;
//      nResyncMarkerSpacing = 1920;
    eResyncMarkerType = RESYNC_MARKER_MB;
    nResyncMarkerSpacing = 50;
    enableHEC = OMX_TRUE;
  }
  else if (m_sProfile.eCodec == OMX_VIDEO_CodingH263)
  {
//H263
    //eResyncMarkerType = RESYNC_MARKER_GOB;
    eResyncMarkerType = RESYNC_MARKER_NONE;
    nResyncMarkerSpacing = 0;
  }
  else if (m_sProfile.eCodec == OMX_VIDEO_CodingAVC)
  {
//H264
//      eResyncMarkerType = RESYNC_MARKER_BYTE;
//      nResyncMarkerSpacing = 1920;

    eResyncMarkerType = RESYNC_MARKER_NONE;
    nResyncMarkerSpacing = 0;

  }

  OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE errorCorrection; //OMX_IndexParamVideoErrorCorrection
  errorCorrection.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
  result = OMX_GetParameter(m_hHandle,
                            (OMX_INDEXTYPE) OMX_IndexParamVideoErrorCorrection,
                            (OMX_PTR) &errorCorrection);

  errorCorrection.bEnableRVLC = OMX_FALSE;
  errorCorrection.bEnableDataPartitioning = OMX_FALSE;

  if (eResyncMarkerType != RESYNC_MARKER_NONE) {
    if ((eResyncMarkerType == RESYNC_MARKER_BYTE) &&
      (m_sProfile.eCodec == OMX_VIDEO_CodingMPEG4)) {
      errorCorrection.bEnableResync = OMX_TRUE;
      errorCorrection.nResynchMarkerSpacing = nResyncMarkerSpacing;
      errorCorrection.bEnableHEC = enableHEC;
    }
    else if ((eResyncMarkerType == RESYNC_MARKER_BYTE) &&
      (m_sProfile.eCodec == OMX_VIDEO_CodingAVC)){
      errorCorrection.bEnableResync = OMX_TRUE;
      errorCorrection.nResynchMarkerSpacing = nResyncMarkerSpacing;
    }
    else if ((eResyncMarkerType == RESYNC_MARKER_GOB) &&
      (m_sProfile.eCodec == OMX_VIDEO_CodingH263)){
      errorCorrection.bEnableResync = OMX_FALSE;
      errorCorrection.nResynchMarkerSpacing = nResyncMarkerSpacing;
      errorCorrection.bEnableDataPartitioning = OMX_TRUE;
    }
    result = OMX_SetParameter(m_hHandle,
                          (OMX_INDEXTYPE) OMX_IndexParamVideoErrorCorrection,
                          (OMX_PTR) &errorCorrection);
    CHK(result);
  }

  if (eResyncMarkerType == RESYNC_MARKER_MB){
    if (m_sProfile.eCodec == OMX_VIDEO_CodingAVC){
      OMX_VIDEO_PARAM_AVCTYPE avcdata;
      avcdata.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
      result = OMX_GetParameter(m_hHandle,
                                OMX_IndexParamVideoAvc,
                                (OMX_PTR) &avcdata);
      CHK(result);
      if (result == OMX_ErrorNone)
      {
        avcdata.nSliceHeaderSpacing = nResyncMarkerSpacing;
        result = OMX_SetParameter(m_hHandle,
                                 OMX_IndexParamVideoAvc,
                                 (OMX_PTR) &avcdata);
        CHK(result);

      }
    }
    else if(m_sProfile.eCodec == OMX_VIDEO_CodingMPEG4){
      OMX_VIDEO_PARAM_MPEG4TYPE mp4;
      mp4.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
      result = OMX_GetParameter(m_hHandle,
                                OMX_IndexParamVideoMpeg4,
                                (OMX_PTR) &mp4);
      CHK(result);

      if (result == OMX_ErrorNone)
      {
        mp4.nSliceHeaderSpacing = nResyncMarkerSpacing;
        result = OMX_SetParameter(m_hHandle,
                                 OMX_IndexParamVideoMpeg4,
                                 (OMX_PTR) &mp4);
        CHK(result);
      }
    }
  }

///////////////////E R R O R C O R R E C T I O N ///////////////////

///////////////////I N T R A R E F R E S H///////////////////
  bool bEnableIntraRefresh = OMX_TRUE;

  if (result == OMX_ErrorNone)
  {
    OMX_VIDEO_PARAM_INTRAREFRESHTYPE ir; // OMX_IndexParamVideoIntraRefresh
    ir.nPortIndex = (OMX_U32) PORT_INDEX_OUT; // output
    result = OMX_GetParameter(m_hHandle,
                             OMX_IndexParamVideoIntraRefresh,
                             (OMX_PTR) &ir);
    if (result == OMX_ErrorNone)
    {
      if (bEnableIntraRefresh)
      {
        ir.eRefreshMode = OMX_VIDEO_IntraRefreshCyclic;
        ir.nCirMBs = 5;
        result = OMX_SetParameter(m_hHandle,
                                 OMX_IndexParamVideoIntraRefresh,
                                 (OMX_PTR) &ir);
        CHK(result);
      }
    }
  }

#if 0 //framepacking is only for stereo video
///////////////////FRAMEPACKING DATA///////////////////
  OMX_QCOM_FRAME_PACK_ARRANGEMENT framePackingArrangement;
  FILE *m_pConfigFile;
  char m_configFilename [128] = "/data/configFile.cfg";
  memset(&framePackingArrangement, 0, sizeof(framePackingArrangement));
  m_pConfigFile = fopen(m_configFilename, "r");
  if (m_pConfigFile != NULL)
  {
    //read all frame packing data
    framePackingArrangement.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
    int totalSizeToRead = FRAME_PACK_SIZE * sizeof(OMX_U32);
    char *pFramePack = (char *) &(framePackingArrangement.id);
    while ( ( (fscanf(m_pConfigFile, "%d", pFramePack)) != EOF ) &&
          (totalSizeToRead != 0) )
    {
      //printf("Addr = %p, Value read = %d, sizeToRead remaining=%d\n",
      //       pFramePack, *pFramePack, totalSizeToRead);
      pFramePack += sizeof(OMX_U32);
      totalSizeToRead -= sizeof(OMX_U32);
    }
    //close the file.
    fclose(m_pConfigFile);

    printf("Frame Packing data from config file:\n");
    PrintFramePackArrangement(framePackingArrangement);
  }
  else
  {
    D("\n Config file does not exist or could not be opened.");
    //set the default values
    framePackingArrangement.nSize = sizeof(OMX_QCOM_FRAME_PACK_ARRANGEMENT);
    framePackingArrangement.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
    framePackingArrangement.id = 123;
    framePackingArrangement.cancel_flag = false;
    framePackingArrangement.type = 3;
    framePackingArrangement.quincunx_sampling_flag = false;
    framePackingArrangement.content_interpretation_type = 0;
    framePackingArrangement.spatial_flipping_flag = true;
    framePackingArrangement.frame0_flipped_flag = false;
    framePackingArrangement.field_views_flag = false;
    framePackingArrangement.current_frame_is_frame0_flag = false;
    framePackingArrangement.frame0_self_contained_flag = true;
    framePackingArrangement.frame1_self_contained_flag = false;
    framePackingArrangement.frame0_grid_position_x = 3;
    framePackingArrangement.frame0_grid_position_y = 15;
    framePackingArrangement.frame1_grid_position_x = 11;
    framePackingArrangement.frame1_grid_position_y = 7;
    framePackingArrangement.reserved_byte = 0;
    framePackingArrangement.repetition_period = 16381;
    framePackingArrangement.extension_flag = false;

    printf("Frame Packing Defaults :\n");
    PrintFramePackArrangement(framePackingArrangement);
  }
  result = OMX_SetConfig(m_hHandle,
            (OMX_INDEXTYPE)OMX_QcomIndexConfigVideoFramePackingArrangement,
            (OMX_PTR) &framePackingArrangement);
  CHK(result);
#endif

//////////////////////OMX_VIDEO_PARAM_INTRAREFRESHTYPE///////////////////

  OMX_CONFIG_FRAMERATETYPE enc_framerate; // OMX_IndexConfigVideoFramerate
  enc_framerate.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
  enc_framerate.nSize = sizeof(OMX_CONFIG_FRAMERATETYPE);

  result = OMX_GetConfig(m_hHandle,
                         OMX_IndexConfigVideoFramerate,
                         &enc_framerate);
  CHK(result);
  FloatToQ16(enc_framerate.xEncodeFramerate, m_sProfile.nFramerate);
  result = OMX_SetConfig(m_hHandle,
                         OMX_IndexConfigVideoFramerate,
                         &enc_framerate);
  CHK(result);
  return OMX_ErrorNone;
}
////////////////////////////////////////////////////////////////////////////////
void SendMessage(MsgId id, MsgData* data)
{
  pthread_mutex_lock(&m_mutex);
  if (m_sMsgQ.size >= MAX_MSG)
  {
    E("main msg m_sMsgQ is full");
    return;
  }
  m_sMsgQ.q[(m_sMsgQ.head + m_sMsgQ.size) % MAX_MSG].id = id;
  if (data)
    m_sMsgQ.q[(m_sMsgQ.head + m_sMsgQ.size) % MAX_MSG].data = *data;
  ++m_sMsgQ.size;
  pthread_cond_signal(&m_signal);
  pthread_mutex_unlock(&m_mutex);
}
////////////////////////////////////////////////////////////////////////////////
void PopMessage(Msg* msg)
{
  pthread_mutex_lock(&m_mutex);
  while (m_sMsgQ.size == 0)
  {
    pthread_cond_wait(&m_signal, &m_mutex);
  }
  *msg = m_sMsgQ.q[m_sMsgQ.head];
  --m_sMsgQ.size;
  m_sMsgQ.head = (m_sMsgQ.head + 1) % MAX_MSG;
  pthread_mutex_unlock(&m_mutex);
}
////////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE EVT_CB(OMX_IN OMX_HANDLETYPE hComponent,
                     OMX_IN OMX_PTR pAppData,
                     OMX_IN OMX_EVENTTYPE eEvent,
                     OMX_IN OMX_U32 nData1,
                     OMX_IN OMX_U32 nData2,
                     OMX_IN OMX_PTR pEventData)
{
#define SET_STATE(eState)                                   \
  case eState:                                             \
  {                                                     \
    D("" # eState " complete");                        \
    m_eState = eState;                                 \
    break;                                             \
  }

  if (eEvent == OMX_EventCmdComplete)
  {
    if ((OMX_COMMANDTYPE) nData1 == OMX_CommandStateSet)
    {
      switch ((OMX_STATETYPE) nData2)
      {
        SET_STATE(OMX_StateLoaded);
        SET_STATE(OMX_StateIdle);
        SET_STATE(OMX_StateExecuting);
        SET_STATE(OMX_StateInvalid);
        SET_STATE(OMX_StateWaitForResources);
        SET_STATE(OMX_StatePause);
        default:
          E("invalid state %d", (int) nData2);
      }
    }
  }

  else if (eEvent == OMX_EventError)
  {
    E("OMX_EventError");
  }

  else
  {
    E("unexpected event %d", (int) eEvent);
  }
  return OMX_ErrorNone;
}
////////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE EBD_CB(OMX_IN OMX_HANDLETYPE hComponent,
                     OMX_IN OMX_PTR pAppData,
                     OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
  D("Got EBD callback ts=%lld", pBuffer->nTimeStamp);

  for (int i = 0; i < m_num_in_buffers; i++)
  {
    // mark this buffer ready for use again
    if (m_pInBuffers[i] == pBuffer)
    {

      D("Marked input buffer idx %d as free, buf %p", i, pBuffer->pBuffer);
      m_bInFrameFree[i] = OMX_TRUE;
      break;
    }
  }

  if (m_eMode == MODE_LIVE_ENCODE)
  {
    CameraTest_ReleaseFrame(pBuffer->pBuffer,
                          ((OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO*)pBuffer->pAppPrivate));
  }
  else
  {
    // wake up main thread and tell it to send next frame
    MsgData data;
    data.sBitstreamData.pBuffer = pBuffer;
    SendMessage(MSG_ID_INPUT_FRAME_DONE,
    &data);

  }
  return OMX_ErrorNone;
}
////////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE FBD_CB(OMX_OUT OMX_HANDLETYPE hComponent,
                     OMX_OUT OMX_PTR pAppData,
                     OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  D("Got FBD callback ts=%lld", pBuffer->nTimeStamp);

  static long long prevTime = 0;
  long long currTime = GetTimeStamp();

  m_bWatchDogKicked = true;

  /* Empty Buffers should not be counted */
  if(pBuffer->nFilledLen !=0)
  {
    /* Counting Buffers supplied from OpneMax Encoder */
    m_fbd_cnt++;
    m_tot_bufsize += pBuffer->nFilledLen;
  }
  if (prevTime != 0)
  {
    long long currTime = GetTimeStamp();
    D("FBD_DELTA = %lld\n", currTime - prevTime);
  }
  prevTime = currTime;

  if (m_eMode == MODE_PROFILE)
  {
    // if we are profiling we are not doing file I/O
    // so just give back to encoder
    if (OMX_FillThisBuffer(m_hHandle, pBuffer) != OMX_ErrorNone)
    {
      E("empty buffer failed for profiling");
    }
  }
  else
  {
    // wake up main thread and tell it to write to file
    MsgData data;
    data.sBitstreamData.pBuffer = pBuffer;
    SendMessage(MSG_ID_OUTPUT_FRAME_DONE,
    &data);
  }
  return OMX_ErrorNone;
}
////////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE VencTest_Initialize()
{
  OMX_ERRORTYPE result = OMX_ErrorNone;
  static OMX_CALLBACKTYPE sCallbacks = {EVT_CB, EBD_CB, FBD_CB};
  int i;

  for (i = 0; i < m_num_in_buffers; i++)
  {
    m_pInBuffers[i] = NULL;
  }

  result = OMX_Init();
  CHK(result);

  if (m_sProfile.eCodec == OMX_VIDEO_CodingMPEG4)
  {
    result = OMX_GetHandle(&m_hHandle,
                         (OMX_STRING) "OMX.qcom.video.encoder.mpeg4",
                         NULL,
                         &sCallbacks);
   // CHK(result);
  }
  else if (m_sProfile.eCodec == OMX_VIDEO_CodingH263)
  {
    result = OMX_GetHandle(&m_hHandle,
                          (OMX_STRING) "OMX.qcom.video.encoder.h263",
                          NULL,
                          &sCallbacks);
    CHK(result);
  }
  else if (m_sProfile.eCodec == OMX_VIDEO_CodingVP8)
  {
    result = OMX_GetHandle(&m_hHandle,
                          (OMX_STRING) "OMX.qcom.video.encoder.vp8",
                          NULL,
                          &sCallbacks);
    CHK(result);
  }
  else if (m_sProfile.eCodec == OMX_VIDEO_CodingHEVC)
  {
    result = OMX_GetHandle(&m_hHandle,
                          (OMX_STRING) "OMX.qcom.video.encoder.hevc",
                          NULL,
                          &sCallbacks);
    CHK(result);

  }
  else
  {
    result = OMX_GetHandle(&m_hHandle,
                          (OMX_STRING) "OMX.qcom.video.encoder.avc",
                          NULL,
                          &sCallbacks);
    CHK(result);
  }


  result = ConfigureEncoder();

  CHK(result);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE VencTest_RegisterYUVBuffer(OMX_BUFFERHEADERTYPE** ppBufferHeader,
                                         OMX_U8 *pBuffer,
                                         OMX_PTR pAppPrivate)
{
  OMX_ERRORTYPE result = OMX_ErrorNone;
#if 0
   D("register buffer");
   if ((result = OMX_AllocateBuffer(m_hHandle,
                               ppBufferHeader,
                               (OMX_U32) PORT_INDEX_IN,
                               pAppPrivate,
                               m_sProfile.nFrameBytes
                               )) != OMX_ErrorNone)
   {
      E("use buffer failed");
   }
   else
   {
     E("Allocate Buffer Success %x", (*ppBufferHeader)->pBuffer);
   }
#endif
  D("register buffer to OMX");
  if (m_eMetaMode) {
    OMX_S32 nFds = 1;
    OMX_S32 nInts = 3;
    D("Calling AllocateBuffer for Input port with metamode");
    result = OMX_AllocateBuffer(m_hHandle, ppBufferHeader,
      (OMX_U32) PORT_INDEX_IN, pAppPrivate, sizeof(MetaBuffer));
    (*ppBufferHeader)->pInputPortPrivate = pBuffer;
    MetaBuffer *pMetaBuffer = (MetaBuffer *)((*ppBufferHeader)->pBuffer);
    if (pMetaBuffer) {
      NativeHandle* pMetaHandle = (NativeHandle*)calloc((
        sizeof(NativeHandle)+ sizeof(OMX_S32)*(nFds + nInts)), 1);
      pMetaBuffer->meta_handle = pMetaHandle;
      pMetaBuffer->buffer_type = CameraSource;
      D("use metamode for Input port");
    }
  }else{
    D("Calling UseBuffer for Input port");
    result = OMX_UseBuffer(m_hHandle,
                               ppBufferHeader,
                               (OMX_U32) PORT_INDEX_IN,
                               pAppPrivate,
                               m_sProfile.nFrameBytes,
                               pBuffer);
    if (result != OMX_ErrorNone) {
      E("use buffer failed");
    }
  }

  return result;
}
////////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE VencTest_EncodeFrame(void* pYUVBuff,
                                   long long nTimeStamp)
{
  OMX_ERRORTYPE result = OMX_ErrorUndefined;
  D("calling OMX empty this buffer");
  for (int i = 0; i < m_num_in_buffers; i++)
  {
    if (pYUVBuff == m_pInBuffers[i]->pBuffer)
    {
      m_pInBuffers[i]->nTimeStamp = nTimeStamp;
      D("Sending Buffer - %p", m_pInBuffers[i]->pBuffer);
      result = OMX_EmptyThisBuffer(m_hHandle,
                                   m_pInBuffers[i]);
      /* Counting Buffers supplied to OpenMax Encoder */
      if(OMX_ErrorNone == result)
        m_ebd_cnt++;
      CHK(result);
      break;
    }
  }
  return result;
}
////////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE VencTest_Exit(void)
{
  int i;
  OMX_ERRORTYPE result = OMX_ErrorNone;
  D("trying to exit venc");

  D("going to idle state");
  SetState(OMX_StateIdle);


  D("going to loaded state");
  //SetState(OMX_StateLoaded);
  OMX_SendCommand(m_hHandle,
                  OMX_CommandStateSet,
                  (OMX_U32) OMX_StateLoaded,
                  NULL);

  for (i = 0; i < m_num_in_buffers; i++)
  {
    D("free buffer");
    if (m_pInBuffers[i]->pBuffer)
    {
      // free(m_pInBuffers[i]->pBuffer);
       result = OMX_FreeBuffer(m_hHandle,
                               PORT_INDEX_IN,
                               m_pInBuffers[i]);
       CHK(result);
    }
    else
    {
      E("buffer %d is null", i);
      result = OMX_ErrorUndefined;
      CHK(result);
    }
  }
  for (i = 0; i < m_num_out_buffers; i++)
  {
    D("free buffer");
    if (m_pOutBuffers[i]->pBuffer)
    {
      free(m_pOutBuffers[i]->pBuffer);
      result = OMX_FreeBuffer(m_hHandle,
                              PORT_INDEX_OUT,
                              m_pOutBuffers[i]);
      CHK(result);

    }
    else
    {
      E("buffer %d is null", i);
      result = OMX_ErrorUndefined;
      CHK(result);
    }
  }

  while (m_eState != OMX_StateLoaded)
  {
    sleep(1);
  }

  OMX_FreeHandle(m_hHandle);
  D("component_deinit...");
  result = OMX_Deinit();
  CHK(result);

  D("venc is exiting...");
  return result;
}
////////////////////////////////////////////////////////////////////////////////

void VencTest_ReadDynamicConfigMsg()
{
  char frame_n[8], config[16], param[8];
  char *dest = frame_n;
  bool end = false;
  int cntr, nparam = 0;
  memset(&m_dynamic_config, 0, sizeof(struct DynamicConfig));
  do
  {
    cntr = -1;
    do
    {
      dest[++cntr] = fgetc(m_pDynConfFile);
    } while(dest[cntr] != ' ' && dest[cntr] != '\t' && dest[cntr] != '\n' && dest[cntr] != '\r' && !feof(m_pDynConfFile));
    if (dest[cntr] == '\n' || dest[cntr] == '\r')
      end = true;
    if (dest == frame_n)
      dest = config;
    else if (dest == config)
      dest = param;
    else
      end = true;
    nparam++;
  } while (!end && !feof(m_pDynConfFile));

  if (nparam > 1)
  {
    m_dynamic_config.pending = true;
    m_dynamic_config.frame_num = atoi(frame_n);
    D("dynamic config frame num: %d\n", m_dynamic_config.frame_num);
    if (!strcmp(config, "bitrate"))
    {
      m_dynamic_config.config_param = OMX_IndexConfigVideoBitrate;
      m_dynamic_config.config_data.bitrate.nPortIndex = PORT_INDEX_OUT;
      m_dynamic_config.config_data.bitrate.nEncodeBitrate = strtoul(param, NULL, 10);
      D("dynamic config bitrate: %d\n", m_dynamic_config.config_data.bitrate.nEncodeBitrate);
    }
    else if (!strcmp(config, "framerate"))
    {
      m_dynamic_config.config_param = OMX_IndexConfigVideoFramerate;
      m_dynamic_config.config_data.framerate.nPortIndex = PORT_INDEX_OUT;
      m_dynamic_config.config_data.f_framerate = atof(param);
      D("dynamic config framerate: %f\n", m_dynamic_config.config_data.f_framerate);
    }
    else if (!strcmp(config, "iperiod"))
    {
      m_dynamic_config.config_param = (OMX_INDEXTYPE)QOMX_IndexConfigVideoIntraperiod;
      m_dynamic_config.config_data.intraperiod.nPortIndex = PORT_INDEX_OUT;
      m_dynamic_config.config_data.intraperiod.nPFrames = strtoul(param, NULL, 10) - 1;
      m_dynamic_config.config_data.intraperiod.nIDRPeriod = 1; // This value is ignored in OMX component
      D("dynamic config intraperiod.nPFrames: %d\n", m_dynamic_config.config_data.intraperiod.nPFrames);
    }
    else if (!strcmp(config, "ivoprefresh"))
    {
      m_dynamic_config.config_param = OMX_IndexConfigVideoIntraVOPRefresh;
      m_dynamic_config.config_data.intravoprefresh.nPortIndex = PORT_INDEX_OUT;
      m_dynamic_config.config_data.intravoprefresh.IntraRefreshVOP = OMX_TRUE;
      D("dynamic config ivoprefresh\n");
    }
    else
    {
      E("UNKNOWN CONFIG PARAMETER: %s!", config);
      m_dynamic_config.pending = false;
    }
  }
  else if (feof(m_pDynConfFile))
  {
    fclose(m_pDynConfFile);
    m_pDynConfFile = NULL;
  }
}

void VencTest_ProcessDynamicConfigurationFile()
{
  do
  {
    if (m_dynamic_config.pending)
    {
      if(m_nFrameIn == m_dynamic_config.frame_num)
      {
        if (m_dynamic_config.config_param == OMX_IndexConfigVideoFramerate)
        {
          m_sProfile.nFramerate = m_dynamic_config.config_data.f_framerate;
          FloatToQ16(m_dynamic_config.config_data.framerate.xEncodeFramerate,
                        m_sProfile.nFramerate);
        }
        if (OMX_SetConfig(m_hHandle, m_dynamic_config.config_param,
            &m_dynamic_config.config_data) != OMX_ErrorNone)
          E("ERROR: Setting dynamic config to OMX param[0x%x]", m_dynamic_config.config_param);
        m_dynamic_config.pending = false;
      }
      else if (m_nFrameIn > m_dynamic_config.frame_num)
      {
        E("WARNING: Config change requested in passed frame(%d)", m_dynamic_config.frame_num);
        m_dynamic_config.pending = false;
      }
    }
    if (!m_dynamic_config.pending)
      VencTest_ReadDynamicConfigMsg();
  } while (!m_dynamic_config.pending && m_pDynConfFile);
}

////////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE VencTest_ReadAndEmpty(OMX_BUFFERHEADERTYPE* pYUVBuffer)
{
  OMX_ERRORTYPE result = OMX_ErrorNone;
  int i, lscanl, lstride, cscanl, cstride, height, width;
  int bytes = 0, read_bytes = 0;
  OMX_U8 *yuv = NULL;
  OMX_U8 *start = NULL;
  height = m_sProfile.nFrameHeight;
  width = m_sProfile.nFrameWidth;
  lstride = VENUS_Y_STRIDE(COLOR_FMT_NV12, width);
  lscanl = VENUS_Y_SCANLINES(COLOR_FMT_NV12, height);
  cstride = VENUS_UV_STRIDE(COLOR_FMT_NV12, width);
  cscanl = VENUS_UV_SCANLINES(COLOR_FMT_NV12, height);
  if (m_eMetaMode) {
    yuv = (OMX_U8 *)pYUVBuffer->pInputPortPrivate;
    start = (OMX_U8 *)pYUVBuffer->pInputPortPrivate;
  }else{
    yuv = pYUVBuffer->pBuffer;
    start = pYUVBuffer->pBuffer;
  }
  for(i = 0; i < height; i++) {
    bytes = read(m_nInFd, yuv, width);
    if (bytes != width) {
      E("read failed: %d != %d\n", bytes, width);
      return OMX_ErrorUndefined;
    }
    read_bytes += bytes;
    yuv += lstride;
  }
  yuv = start + (lscanl * lstride);
  for (i = 0; i < ((height + 1) >> 1); i++) {
    bytes = read(m_nInFd, yuv, width);
    if (bytes != width) {
      E("read failed: %d != %d\n", bytes, width);
      return OMX_ErrorUndefined;
    }
    read_bytes += bytes;
    yuv += cstride;
  }
  E("\n\nActual read bytes: %d from file, which will be filled into NV12 buf area size: %d\n\n\n", read_bytes, m_sProfile.nFrameRead);
  if(m_eMetaMode){
    OMX_S32 nFds = 1;
    OMX_S32 nInts = 3;
    int offset = 0;
    int maxsize = 0;
    OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO* pMem = (OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO*)(pYUVBuffer->pAppPrivate);
    MetaBuffer *pMetaBuffer = (MetaBuffer *)(pYUVBuffer->pBuffer);
    NativeHandle* pMetaHandle = NULL;
    if (!pMem || !pMetaBuffer) {
      return OMX_ErrorUndefined;
    }
    pMetaHandle = pMetaBuffer->meta_handle;
    if (!pMetaHandle) {
      return OMX_ErrorUndefined;
    }
    pMetaHandle->version = sizeof(NativeHandle);
    pMetaHandle->numFds = nFds;
    pMetaHandle->numInts = nInts;
    pMetaHandle->data[0] = pMem->pmem_fd;
    pMetaHandle->data[1] = 0; //offset
    pMetaHandle->data[2] = m_sProfile.nFrameBytes;
    pMetaHandle->data[3] = ITUR601;
    // if (pYUVBuffer->port->port_def.format.video.eColorFormat == QOMX_COLOR_FORMATYUV420PackedSemiPlanar32mCompressed){
     //  pMetaHandle->data[3] |= GBM_BO_USAGE_UBWC_ALIGNED_QTI;
    // }
    pMetaBuffer->buffer_type = CameraSource;
    D("metamode data[2] %d", pMetaHandle->data[2]);
  }
  if (m_pDynConfFile)
    VencTest_ProcessDynamicConfigurationFile();
  D("about to call VencTest_EncodeFrame...");
  pthread_mutex_lock(&m_mutex);
  ++m_nFrameIn;
  pYUVBuffer->nFilledLen = m_sProfile.nFrameRead;
  D("Called Buffer with Data filled length %d",pYUVBuffer->nFilledLen);

  result = VencTest_EncodeFrame(pYUVBuffer->pBuffer,
                                 m_nTimeStamp);

  m_nTimeStamp += (1000000) / m_sProfile.nFramerate;
  CHK(result);
  pthread_mutex_unlock(&m_mutex);
  return result;
}
////////////////////////////////////////////////////////////////////////////////
void PreviewCallback(int nFD,
                     int nOffset,
                     void* pPhys,
                     void* pVirt,
                     long long nTimeStamp)
{

  D("================= preview frame %d, phys=0x%x, nTimeStamp(millis)=%lld",
    m_nFrameIn+1, pPhys, (nTimeStamp / 1000));

  if (m_nFrameIn == m_nFramePlay &&
    m_nFramePlay != 0)
  {
    // we will stop camera after last frame is encoded.
    // for now just ignore input frames

    CameraTest_ReleaseFrame(pPhys, pVirt);
    return;
  }

  // see if we should stop
  pthread_mutex_lock(&m_mutex);
  ++m_nFrameIn;
  pthread_mutex_unlock(&m_mutex);


  if (m_eMode == MODE_LIVE_ENCODE)
  {

    OMX_ERRORTYPE result;

    // register new camera buffers with encoder
    int i;
    for (i = 0; i < m_num_in_buffers; i++)
    {
      if (m_pInBuffers[i] != NULL &&
           m_pInBuffers[i]->pBuffer == pPhys)
      {
        break;
      }
      else if (m_pInBuffers[i] == NULL)
      {
        D("registering buffer...");
        result = VencTest_RegisterYUVBuffer(&m_pInBuffers[i],
                                            (OMX_U8*) pPhys,
                                            (OMX_PTR) pVirt); // store virt in app private field
        D("register done");
        CHK(result);
        break;
      }
    }

    if (i == m_num_in_buffers)
    {
      E("There are more camera buffers than we thought");
      CHK(1);
    }

    // encode the yuv frame

    D("StartEncodeTime=%lld", GetTimeStamp());
    result = VencTest_EncodeFrame(pPhys,
                                  nTimeStamp);
    CHK(result);
    // FBTest_DisplayImage(nFD, nOffset);
  }
  else
  {
    // FBTest_DisplayImage(nFD, nOffset);
    CameraTest_ReleaseFrame(pPhys, pVirt);
  }
}
////////////////////////////////////////////////////////////////////////////////
void usage(char* filename)
{
  char* fname = strrchr(filename, (int) '/');
  fname = (fname == NULL) ? filename : fname;

  fprintf(stderr, "usage: %s LIVE <QCIF|QVGA> <MP4|H263> <FPS> <BITRATE> <NFRAMES> <OUTFILE>\n", fname);
  fprintf(stderr, "usage: %s FILE <QCIF|QVGA> <MP4|H263 <FPS> <BITRATE> <NFRAMES> <INFILE> <OUTFILE> ", fname);
  fprintf(stderr, "<Dynamic config file - opt> <Rate Control - opt> <AVC Slice Mode - opt>\n", fname);
  fprintf(stderr, "usage: %s PROFILE <QCIF|QVGA> <MP4|H263 <FPS> <BITRATE> <NFRAMES> <INFILE>\n", fname);
  fprintf(stderr, "usage: %s PREVIEW <QCIF|QVGA> <FPS> <NFRAMES>\n", fname);
  fprintf(stderr, "usage: %s DISPLAY <QCIF|QVGA> <FPS> <NFRAMES> <INFILE>\n", fname);
  fprintf(stderr, "\n       BITRATE - bitrate in kbps\n");
  fprintf(stderr, "       FPS - frames per second\n");
  fprintf(stderr, "       NFRAMES - number of frames to play, 0 for infinite\n");
  fprintf(stderr, "       RateControl (Values 0 - 4 for RC_OFF, RC_CBR_CFR, RC_CBR_VFR, RC_VBR_CFR, RC_VBR_VFR\n");
  exit(1);
}

bool parseWxH(char *str, OMX_U32 *width, OMX_U32 *height)
{
  bool parseOK = false;
  const char delimiters[] = " x*,";
  char *token, *dupstr, *temp;
  OMX_U32 w, h;

  dupstr = strdup(str);
  token = strtok_r(dupstr, delimiters, &temp);
  if (token)
  {
    w = strtoul(token, NULL, 10);
    token = strtok_r(NULL, delimiters, &temp);
    if (token)
    {
      h = strtoul(token, NULL, 10);
      if (w != ULONG_MAX && h != ULONG_MAX)
      {
#ifdef MAX_RES_720P
        if ((w * h >> 8) <= 3600)
        {
          parseOK = true;
          *width = w;
          *height = h;
        }
#else
        if ((w * h >> 8) <= 8160)
        {
          parseOK = true;
          *width = w;
          *height = h;
        }
#endif
        else
          E("\nInvalid dimensions %dx%d",w,h);
      }
    }
  }
  free(dupstr);
  return parseOK;
}

void help()
{
  printf("\n\n");
  printf("=============================\n");
  printf("mm-venc-omx-test args... \n");
  printf("=============================\n\n");
  printf("      -m mode (live, file). Only support file mode now\n");
  printf("      -t encode type (mpeg4, h263, h264, vp8, hevc). h264 and hevc are verified\n");
  printf("      -w width\n");
  printf("      -h height\n");
  printf("      -f fps\n");
  printf("      -b bitrate. Unit is bit per second\n");
  printf("      -n number of frames to encode\n");
  printf("      -i infile (Stored as NV12 format only)\n");
  printf("      -o outfile\n");
  printf("      -c ratecontrol option\n");
  printf("         (Values 0 - 4 for RC_OFF, RC_CBR_CFR, RC_CBR_VFR, RC_VBR_CFR, RC_VBR_VFR)\n");
  printf("      -M metamode (value 0 or 1, 0 not use metamode, 1 use metamode. 0 is default value if not specify -M)\n");
  printf("      -r rotation (90, 180, 270). (Not verify)\n");
  printf("      -d dynamic control file (Not verify)\n");
  printf("      --scaling-width scale_w (Not verify)\n");
  printf("      --scaling-heigth scale_h (Not verify)\n");
  printf("      --help  Print this menu\n");
  printf("=============================\n");
  printf("Example:\n");
  printf("program -m file -t h264 -w 1280 -h 720 -f 30 -b 5000000 -c 3 -n 300 -i jelly_nv12.yuv -o 5mbps.h264\n");
  printf("program -m file -t hevc -w 1280 -h 720 -f 30 -b 5000000 -c 3 -n 300 -i jelly_nv12.yuv -o 5mbps.h265\n");
  printf("=============================\n\n\n");
}

static int parse_args(int argc, char **argv)
{
  int command;
  int rc_opt = 0;

  struct option longopts[] = {
    { "mode", required_argument, NULL, 'm'},
    { "type", required_argument, NULL, 't'},
    { "width", required_argument, NULL, 'w'},
    { "height",      required_argument, NULL, 'h'},
    { "fps",     required_argument, NULL, 'f'},
    { "bitrate",      required_argument, NULL, 'b'},
    { "nframes",      required_argument, NULL, 'n'},
    { "ratecontrol",      required_argument, NULL, 'c'},
    { "infile",      required_argument, NULL, 'i'},
    { "outfile",      required_argument, NULL, 'o'},
    { "rotation",      required_argument, NULL, 'r'},
    { "metamode",    required_argument, NULL, 'M'},
    { "help",        no_argument,       NULL, 0},
    { "scaling-width",        required_argument,       NULL, 1},
    { "scaling-height",        required_argument,       NULL, 2},
    { NULL,          0,                 NULL,  0}
  };
  //default
  m_sProfile.eControlRate = OMX_Video_ControlRateVariable;
  m_sProfile.nUserProfile = 0;

  while ((command = getopt_long(argc, argv, "m:t:w:h:f:b:n:c:i:o:r:d:M:", longopts,
    NULL)) != -1) {
    switch (command) {
      case 'm':
        if (!strcmp("preview", optarg)) {
          m_eMode = MODE_PREVIEW;
        }
        else if (!strcmp("display", optarg)) {
          m_eMode = MODE_DISPLAY;
        }
        else if (!strcmp("live", optarg)) {
          m_eMode = MODE_LIVE_ENCODE;
        }
        else if (!strcmp("file", optarg)) {
          m_eMode = MODE_FILE_ENCODE;
        }
        else {
          E("Invalid test mode");
          return -1;
        }
        break;
      case 't':
        if (!strcmp(optarg, "mpeg4")) {
          m_sProfile.eCodec = OMX_VIDEO_CodingMPEG4;
        }
        else if (!strcmp(optarg, "h263")) {
          m_sProfile.eCodec = OMX_VIDEO_CodingH263;
        }
        else if (!strcmp(optarg, "h264")) {
          m_sProfile.eCodec = OMX_VIDEO_CodingAVC;
        }
        else if (!strcmp(optarg, "vp8")) {
          m_sProfile.eCodec = OMX_VIDEO_CodingVP8;
        }
        else if (!strcmp(optarg, "hevc")) {
          m_sProfile.eCodec = OMX_VIDEO_CodingHEVC;
        }
        else {
          E("Invaid encode type");
          return -1;
        }
        break;
      case 'w':
        m_sProfile.nFrameWidth = atoi(optarg);
        break;
      case 'h':
        m_sProfile.nFrameHeight = atoi(optarg);
        break;
      case 'f':
        m_sProfile.nFramerate = atof(optarg);
        break;
      case 'b':
        m_sProfile.nBitrate = atoi(optarg);
        break;
      case 'n':
        m_nFramePlay = atoi(optarg);
        if (m_nFramePlay <= 0) {
          E ("Invalid frame number");
          return -1;
        }
        break;
      case 'c':
        rc_opt = atoi(optarg);
        switch (rc_opt)
        {
          case 0:
            m_sProfile.eControlRate  = OMX_Video_ControlRateDisable ;//VENC_RC_NONE
            break;
          case 1:
            m_sProfile.eControlRate  = OMX_Video_ControlRateConstant;//VENC_RC_CBR_CFR
            break;
          case 2:
            m_sProfile.eControlRate  = OMX_Video_ControlRateConstantSkipFrames;//VENC_RC_CBR_VFR
            break;
          case 3:
            m_sProfile.eControlRate  =OMX_Video_ControlRateVariable ;//VENC_RC_VBR_CFR
            break;
          case 4:
            m_sProfile.eControlRate  = OMX_Video_ControlRateVariableSkipFrames;//VENC_RC_VBR_VFR
            break;
          default:
            E("invalid rate control selection");
            m_sProfile.eControlRate = OMX_Video_ControlRateVariable; //VENC_RC_VBR_CFR
            break;
        }
        break;
      case 'i':
        m_sProfile.cInFileName = optarg;
        break;
      case 'o':
        m_sProfile.cOutFileName = optarg;
        break;
      case 'r':
        m_rotation = atoi(optarg);
        if (m_rotation != 90 && m_rotation != 180 && m_rotation != 270) {
          E ("Invalid rotation angle");
          return -1;
        }
        break;
      case 'd':
        m_pDynConfFile = fopen(optarg, "r");
        if (!m_pDynConfFile)
          E("ERROR: Cannot open dynamic config file");
        else
          memset(&m_dynamic_config, 0, sizeof(struct DynamicConfig));
        break;
      case 'M':
        m_eMetaMode = atoi(optarg);
        if (m_eMetaMode != 0 && m_eMetaMode != 1){
          E ("Invalid metamode");
          return -1;
        }
        break;
      case 0:
        help();
        exit(0);
      case 1:
        m_scaling_width = atoi(optarg);
        break;
      case 2:
        m_scaling_height = atoi(optarg);
        break;
      default:
        return -1;
    }
  }
  if (m_sProfile.nFrameWidth > 0 && m_sProfile.nFrameHeight > 0) {
    m_sProfile.eLevel = OMX_VIDEO_MPEG4Level1;
    //only support VENUS NV12 format
    m_sProfile.nFramestride = VENUS_Y_STRIDE(COLOR_FMT_NV12, m_sProfile.nFrameWidth);
    m_sProfile.nFrameScanlines = VENUS_Y_SCANLINES(COLOR_FMT_NV12, m_sProfile.nFrameHeight);
    m_sProfile.nFrameRead = m_sProfile.nFramestride * m_sProfile.nFrameScanlines * 3 / 2;
    m_sProfile.nFrameBytes = VENUS_BUFFER_SIZE(COLOR_FMT_NV12, m_sProfile.nFrameWidth, m_sProfile.nFrameScanlines);

    D("app calculate frame buf sz %d, frame stride*scanline*3/2 sz %d\n", m_sProfile.nFrameBytes, m_sProfile.nFrameRead);
  }
  else {
    E ("Invalid input width or height");
    return -1;
  }

  if (m_sProfile.nFramerate <= 0) {
    E ("Invalid input frame rate");
    return -1;
  }

  return 0;
}

void* Watchdog(void* data)
{
  while (1)
  {
    sleep(1000);
    if (m_bWatchDogKicked == true)
      m_bWatchDogKicked = false;
    else
      E("watchdog has not been kicked. we may have a deadlock");
  }
  return NULL;
}

void open_device ()
{
  D("open mem device\n");
#ifdef USE_ION
#ifdef USE_GBM
  m_device_fd = open (PMEM_DEVICE, O_RDWR | O_CLOEXEC);
  if(m_device_fd < 0)
  {
    E("\nERROR: gbm Device open() Failed");
    return;
  }
  m_gbm_device = gbm_create_device(m_device_fd);
  if (m_gbm_device == NULL)
  {
    close(m_device_fd);
    m_device_fd = -1;
    E("gbm_create_device failed\n");
  }
#else
  m_device_fd = ion_open();
  if(m_device_fd < 0)
  {
    E("\nERROR: ION Device open() Failed");
  }
#endif
#else
#error "Only support ION or ION/GBM mechanism, not support pmem any more!"
  m_device_fd = open(PMEM_DEVICE, O_RDWR);
  if (device_fd < 0)
    E("device open failed\n");;
#endif
}
void close_device ()
{
  D("close mem device\n");
#ifdef USE_GBM
  if (m_gbm_device) {
    gbm_device_destroy(m_gbm_device);
    m_gbm_device = NULL;
  }
#endif
  close(m_device_fd);
  m_device_fd =-1;
}
int main(int argc, char** argv)
{
  OMX_U8* pvirt = NULL;
  int result;
  float enc_time_sec=0.0,enc_time_usec=0.0;

  m_nTimeStamp = 0;
  m_nFrameIn = 0;
  m_nFrameOut = 0;

  printf("PID %d\n",getpid());
  memset(&m_sMsgQ, 0, sizeof(MsgQ));
  memset(&m_sProfile, 0, sizeof(m_sProfile));
  //parseArgs(argc, argv);
  if (parse_args(argc, argv) != 0) {
    E ("Invalid arguments");
    help();
    return 0;
  }

  D("fps=%f, bitrate=%u, width=%u, height=%u, frame bytes=%u",
    m_sProfile.nFramerate,
    m_sProfile.nBitrate,
    m_sProfile.nFrameWidth,
    m_sProfile.nFrameHeight,
    m_sProfile.nFrameBytes);
  D("Frame stride=%u, scanlines=%u, read area=%u",
      m_sProfile.nFramestride,
      m_sProfile.nFrameScanlines,
      m_sProfile.nFrameRead);

  //if (m_eMode != MODE_PREVIEW && m_eMode != MODE_DISPLAY)
  //{
    // pthread_t wd;
    // pthread_create(&wd, NULL, Watchdog, NULL);
  //}

  for (int x = 0; x < m_num_in_buffers; x++)
  {
    // mark all buffers as ready to use
    m_bInFrameFree[x] = OMX_TRUE;
  }


  if (m_eMode != MODE_PROFILE)
  {
    if(!m_sProfile.cOutFileName)
    {
      E("No output file name");
      CHK(1);
    }
    m_nOutFd = fopen(m_sProfile.cOutFileName,"wb");
    if (m_nOutFd == NULL)
    {
      E("could not open output file %s", m_sProfile.cOutFileName);
      CHK(1);
    }
  }

  pthread_mutex_init(&m_mutex, NULL);
  pthread_cond_init(&m_signal, NULL);

  if (m_eMode != MODE_PREVIEW)
  {
    VencTest_Initialize();
  }

#if 1
  if (m_rotation != 0) {
    m_dynamic_config.config_param = OMX_IndexConfigCommonRotate;
    m_dynamic_config.config_data.rotation.nPortIndex = PORT_INDEX_OUT;
    m_dynamic_config.config_data.rotation.nRotation = m_rotation;
    OMX_SetConfig(m_hHandle, m_dynamic_config.config_param, &m_dynamic_config.config_data);
  }
#endif

  ////////////////////////////////////////
  // Camera + Encode
  ////////////////////////////////////////
  if (m_eMode == MODE_LIVE_ENCODE)
  {
    CameraTest_Initialize(m_sProfile.nFramerate,
                          m_sProfile.nFrameWidth,
                          m_sProfile.nFrameHeight,
                          PreviewCallback);
    CameraTest_Run();
  }

  if (m_eMode == MODE_FILE_ENCODE ||
    m_eMode == MODE_PROFILE)
  {
    int i;
    if(!m_sProfile.cInFileName)
    {
      E("No input file name");
      CHK(1);
    }
    m_nInFd = open(m_sProfile.cInFileName, O_RDONLY);
    if (m_nInFd < 0)
    {
      E("could not open input file");
      CHK(1);

    }
    D("going to idle state");
    //SetState(OMX_StateIdle);
    OMX_SendCommand(m_hHandle,
                    OMX_CommandStateSet,
                    (OMX_U32) OMX_StateIdle,
                    NULL);

    OMX_PARAM_PORTDEFINITIONTYPE portDef;

    portDef.nPortIndex = 0;
    result = OMX_GetParameter(m_hHandle, OMX_IndexParamPortDefinition, &portDef);
    CHK(result);
    if (m_eMetaMode) {
      StoreMetaDataInBuffersParams sMetadataMode;
      OMX_INIT_STRUCT(&sMetadataMode, StoreMetaDataInBuffersParams);
      sMetadataMode.nPortIndex = 0;
      sMetadataMode.bStoreMetaData = OMX_TRUE;
      result = OMX_SetParameter(m_hHandle,
          (OMX_INDEXTYPE)OMX_QcomIndexParamVideoMetaBufferMode,
          (OMX_PTR)&sMetadataMode);
      CHK(result);
    }
    open_device();
    D("allocating Input buffers");
    m_num_in_buffers = portDef.nBufferCountActual;
    m_ion_data_array = (struct enc_ion *)calloc(sizeof(struct enc_ion), m_num_in_buffers);
    if(m_ion_data_array == NULL)
    {
      CHK(1);
    }
    for (i = 0; i < portDef.nBufferCountActual; i++)
    {
      if (!m_eMetaMode)
      {
        OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO* pMem = new OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO;
        pvirt = (OMX_U8*)PmemMalloc(pMem, m_sProfile.nFrameBytes, &m_ion_data_array[i]);

        if(pvirt == NULL)
        {
          CHK(1);
        }
        result = VencTest_RegisterYUVBuffer(&m_pInBuffers[i],
                                               (OMX_U8*) pvirt,
                                               (OMX_PTR) pMem);
      }
      else
      {
        //For metamode, needn't allocate real frame memory before register buffer to omx il.
        //Only before encoding that frame, need to prepare real memory and attach to "omx buf".
        result = VencTest_RegisterYUVBuffer(&m_pInBuffers[i],
                                               NULL,
                                               NULL);
      }
      CHK(result);
    }
  }
  else if (m_eMode == MODE_LIVE_ENCODE)
  {
    D("going to idle state");
    //SetState(OMX_StateIdle);
    OMX_SendCommand(m_hHandle,
                    OMX_CommandStateSet,
                    (OMX_U32) OMX_StateIdle,
                    NULL);
  }

  int i;
  OMX_PARAM_PORTDEFINITIONTYPE portDef;

  portDef.nPortIndex = 1;
  result = OMX_GetParameter(m_hHandle, OMX_IndexParamPortDefinition, &portDef);
  CHK(result);

  D("allocating & calling usebuffer for Output port");
  m_num_out_buffers = portDef.nBufferCountActual;
  for (i = 0; i < portDef.nBufferCountActual; i++)
  {
    void* pBuff;

    pBuff = malloc(portDef.nBufferSize);
    D("portDef.nBufferSize = %d ",portDef.nBufferSize);
    result = OMX_UseBuffer(m_hHandle,
                          &m_pOutBuffers[i],
                          (OMX_U32) PORT_INDEX_OUT,
                          NULL,
                          portDef.nBufferSize,
                          (OMX_U8*) pBuff);
    CHK(result);
  }
  D("allocate done");

  // D("Going to state " # eState"...");

  while (m_eState != OMX_StateIdle)
  {
    sleep(1);
  }
  //D("Now in state " # eState);


  D("going to executing state");
  SetState(OMX_StateExecuting);
  for (i = 0; i < m_num_out_buffers; i++)
  {
    D("filling buffer %d", i);
    result = OMX_FillThisBuffer(m_hHandle, m_pOutBuffers[i]);
    //sleep(1000);
    CHK(result);
  }
  if (m_eMetaMode)
  {

    for (i = 0; i < m_num_in_buffers; i++)
    {
      OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO* pMem = new OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO;
      pvirt = (OMX_U8*)PmemMalloc(pMem, m_sProfile.nFrameBytes, &m_ion_data_array[i]);

      if(pvirt == NULL)
      {
        CHK(1);
      }
      m_pInBuffers[i]->pAppPrivate = pMem;
      m_pInBuffers[i]->pInputPortPrivate = pvirt;
    }
  }
  if (m_eMode == MODE_FILE_ENCODE)
  {
    // encode the first frame to kick off the whole process
    VencTest_ReadAndEmpty(m_pInBuffers[0]);
    //  FBTest_DisplayImage(((PmemBuffer*) m_pInBuffers[0]->pAppPrivate)->fd,0);
  }

  if (m_eMode == MODE_PROFILE)
  {
    int i;

    // read several frames into memory
    D("reading frames into memory");
    for (i = 0; i < m_num_in_buffers; i++)
    {
      D("[%d] address 0x%x",i, m_pInBuffers[i]->pBuffer);
#ifdef MAX_RES_720P
      read(m_nInFd,
           m_pInBuffers[i]->pBuffer,
           m_sProfile.nFrameBytes);
#else
      // read Y first
      if(read(m_nInFd, m_pInBuffers[i]->pBuffer,
           m_sProfile.nFrameWidth*m_sProfile.nFrameHeight) <= 0) {
        E("Error while reading frame");
        break;
      }

      // check alignment for offset to C
      OMX_U32 offset_to_c = m_sProfile.nFrameWidth * m_sProfile.nFrameHeight;

      const OMX_U32 C_2K = (1024*2),
      MASK_2K = C_2K-1,
      IMASK_2K = ~MASK_2K;

      if (offset_to_c & MASK_2K)
      {
        // offset to C is not 2k aligned, adjustment is required
        offset_to_c = (offset_to_c & IMASK_2K) + C_2K;
      }

      // read C
      if (read(m_nInFd, m_pInBuffers[i]->pBuffer + offset_to_c,
             m_sProfile.nFrameWidth*m_sProfile.nFrameHeight/2) <= 0) {
        E("Error while reading frame");
        break;
      }

#endif
    }

    // FBTest_Initialize(m_sProfile.nFrameWidth, m_sProfile.nFrameHeight);

    // loop over the mem-resident frames and encode them
    D("beging playing mem-resident frames...");
    for (i = 0; m_nFramePlay == 0 || i < m_nFramePlay; i++)
    {
      int idx = i % m_num_in_buffers;
      if (m_bInFrameFree[idx] == OMX_FALSE)
      {
        int j;
        E("the expected buffer is not free, but lets find another");

        idx = -1;

        // lets see if we can find another free buffer
        for (j = 0; j < m_num_in_buffers; j++)
        {
          if(m_bInFrameFree[j])
          {
            idx = j;
            break;
          }
        }
      }

      // if we have a free buffer let's encode it
      if (idx >= 0)
      {
        D("encode frame %d...m_pInBuffers[idx]->pBuffer=0x%x", i,m_pInBuffers[idx]->pBuffer);
        m_bInFrameFree[idx] = OMX_FALSE;
        VencTest_EncodeFrame(m_pInBuffers[idx]->pBuffer,
                            m_nTimeStamp);
        D("display frame %d...", i);
        //    FBTest_DisplayImage(((PmemBuffer*) m_pInBuffers[idx]->pAppPrivate)->fd,0);
        m_nTimeStamp += 1000000 / m_sProfile.nFramerate;
      }
      else
      {
        E("wow, no buffers are free, performance "
          "is not so good. lets just sleep some more");

      }
      D("sleep for %d microsec", (int)(1000000/m_sProfile.nFramerate));//nFramerate is float.
      sleep (1000000 / m_sProfile.nFramerate);
    }
    // FBTest_Exit();
  }

  Msg msg;
  bool bQuit = false;
  while ((m_eMode == MODE_FILE_ENCODE || m_eMode == MODE_LIVE_ENCODE) &&
        !bQuit)
  {
    PopMessage(&msg);
    switch (msg.id)
    {
      //////////////////////////////////
      // FRAME IS ENCODED
      //////////////////////////////////
      case MSG_ID_INPUT_FRAME_DONE:
        /*pthread_mutex_lock(&m_mutex);
        ++m_nFrameOut;
        if (m_nFrameOut == m_nFramePlay && m_nFramePlay != 0)
        {
          bQuit = true;
        }
        pthread_mutex_unlock(&m_mutex);*/

        if (!bQuit && m_eMode == MODE_FILE_ENCODE)
        {
          D("pushing another frame down to encoder");
          if (VencTest_ReadAndEmpty(msg.data.sBitstreamData.pBuffer))
          {
            // we have read the last frame
            D("main is exiting...");
            bQuit = true;
          }
        }
        break;
      case MSG_ID_OUTPUT_FRAME_DONE:
        int bytes_written;
        bytes_written = fwrite(msg.data.sBitstreamData.pBuffer->pBuffer,
            1, msg.data.sBitstreamData.pBuffer->nFilledLen,
            m_nOutFd);
        D("================ writing frame %d = %d bytes to output file",
            m_nFrameOut+1,
            bytes_written);
        D("StopEncodeTime=%lld", GetTimeStamp());

        result = OMX_FillThisBuffer(m_hHandle,
                   msg.data.sBitstreamData.pBuffer);

        if (result != OMX_ErrorNone)
        {
          CHK(result);
        }

        pthread_mutex_lock(&m_mutex);
        ++m_nFrameOut;
        if (m_nFrameOut == m_nFramePlay && m_nFramePlay != 0)
        {
          bQuit = true;
        }
        pthread_mutex_unlock(&m_mutex);
        break;

      default:
        E("invalid msg id %d", (int) msg.id);
      } // end switch (msg.id)

/*  // TO UNCOMMENT FOR PAUSE TESTINGS
      if(m_nFrameOut == 10)
      {
         E("\nGoing to Pause state\n");
         SetState(OMX_StatePause);
         sleep(3);
//REQUEST AN I FRAME AFTER PAUSE
         OMX_CONFIG_INTRAREFRESHVOPTYPE voprefresh;
         voprefresh.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
         voprefresh.IntraRefreshVOP = OMX_TRUE;
         result = OMX_SetConfig(m_hHandle,
                                   OMX_IndexConfigVideoIntraVOPRefresh,
                                   &voprefresh);
         E("\n OMX_IndexConfigVideoIntraVOPRefresh Set Paramter port");
         CHK(result);
         E("\nGoing to executing state\n");
         SetState(OMX_StateExecuting);
      }
*/
  } // end while (!bQuit)


  if (m_eMode == MODE_LIVE_ENCODE)
  {
    CameraTest_Exit();
    fclose(m_nOutFd);
    m_nOutFd = NULL;
  }
  else if (m_eMode == MODE_FILE_ENCODE ||
           m_eMode == MODE_PROFILE)
  {
    // deallocate pmem buffers
    for (int i = 0; i < m_num_in_buffers; i++)
    {
      if(m_eMetaMode && ((MetaBuffer *)(m_pInBuffers[i]->pBuffer))->meta_handle){
        free(((MetaBuffer *)(m_pInBuffers[i]->pBuffer))->meta_handle);
        ((MetaBuffer *)(m_pInBuffers[i]->pBuffer))->meta_handle = NULL;
      }
      PmemFree((OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO*)m_pInBuffers[i]->pAppPrivate,
               m_pInBuffers[i]->pBuffer,
               m_sProfile.nFrameBytes, &m_ion_data_array[i]);
      delete (OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO*) m_pInBuffers[i]->pAppPrivate;
    }
    if(m_ion_data_array)
      free(m_ion_data_array);
    close_device();
    close(m_nInFd);
    if (m_eMode == MODE_FILE_ENCODE)
    {
      fclose(m_nOutFd);
      m_nOutFd = NULL;
    }
    if (m_pDynConfFile)
    {
      fclose(m_pDynConfFile);
      m_pDynConfFile = NULL;
    }
  }

  if (m_eMode != MODE_PREVIEW)
  {
    D("exit encoder test");
    VencTest_Exit();
  }

  pthread_mutex_destroy(&m_mutex);
  pthread_cond_destroy(&m_signal);

  /* Time Statistics Logging */
  if(0 != m_sProfile.nFramerate)
  {
    enc_time_usec = m_nTimeStamp - (1000000 / m_sProfile.nFramerate);
    enc_time_sec =enc_time_usec/1000000;
    if(0 != enc_time_sec)
    {
      printf("Total Frame Rate: %f",m_ebd_cnt/enc_time_sec);
      printf("\nEncoder Bitrate :%lf Kbps",(m_tot_bufsize*8)/(enc_time_sec*1000));
    }
  }
  else
  {
    printf("\n\n Encode Time is zero");
  }
  printf("\nTotal Number of Frames :%d",m_ebd_cnt);
  printf("\nNumber of dropped frames during encoding:%d\n",m_ebd_cnt-m_fbd_cnt);
  /* End of Time Statistics Logging */

  D("main has exited");
  return 0;
}
