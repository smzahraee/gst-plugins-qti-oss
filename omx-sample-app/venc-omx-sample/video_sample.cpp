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
#include <time.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <errno.h>

/**************************** QTI Extend function **************************/
// Hardware related, for example: width align
#include <vidc/media/msm_media_info.h>

#define QTI_EXT 1

/****************************** debug control ******************************/
#include "video_debug.h"
#include "video_sample.h"
#include "omx_utils.h"
#include "config_utils.h"
#include "cpu_utils.h"

/****************************** Inner data define **************************/

#define CHECK_STATUS(str, result) if (result != 0) \
{ \
  VLOGE("%s", str); \
  FUNCTION_EXIT(); \
  return -1; \
}

// DEBUG log level: LVL_P | LVL_D | LVL_V | LVL_F
uint32_t m_DebugLevelSets = LVL_P;

VideoCodecSetting_t m_Settings;
DynamicConfigure_t * m_DynamicConfigures;
char * m_MemoryMode = "gbm";

#define MAX_FILE_PATH 128
// input file: for example NV12 raw data, include continue NV12 frame data
static char m_InputFileName[MAX_FILE_PATH] = "NV12_1280_720_all.yuv";
static int m_InputFileFd = -1;

// output file: used to store encoded data
static char  m_OutputFileName[MAX_FILE_PATH] = "NV12_1280_720.mp4";
static int m_OutputFileFd = -1;

// used to wait codec component stop notify
static bool m_Running = false;
static pthread_mutex_t m_RunningMutex;
static pthread_cond_t  m_RunningSignal;

// json configure file, used to set codec parameters
static char * m_ConfigFilePath = NULL;
// json configure file, used to ser codec parametes
static char * m_DynamicConfigFilePath = NULL;

// used to sync multi-thread print log time: avoid reentrant GetTimeStamp function
static pthread_mutex_t m_PrintTimeMutex;

// used record statistical data
static int32_t m_EncodeFrameNum = 0;
static float   m_TotalTime = 0.0;
static int32_t m_InputDataSize = 0;
static int32_t m_OutputDataSize = 0;

struct timeval m_StartTime;  // used for print log time
struct timeval m_EncodeStartTime;  // used to compute encode performance
struct timeval m_EncodeEndTime;  // used to compute encode performance

int32_t m_ControlFps = 0;
// refer to enum Mode
int32_t m_TestMode = 0;

int32_t m_MetadataMode = 0;

unsigned long long m_CpuOccupyStartTime = 0;
int m_Pid = 0;

int mDynamicConfigNum = 0;

#ifdef PRINT_TIME
#include <sys/time.h>

static char m_TimeStr[MAX_STR_LEN] = {" "};

// format the time diff between main function start to log output
// for example: 0.123, 1.456
// please take attention about m_StartTime
char * GetTimeStamp() {
  struct timeval now;
  int64_t diff_time;

  pthread_mutex_lock(&m_PrintTimeMutex);

  gettimeofday(&now, NULL);

  diff_time = (now.tv_sec - m_StartTime.tv_sec) * 1000 + \
              (now.tv_usec - m_StartTime.tv_usec) / 1000;

  snprintf(&m_TimeStr[0], MAX_STR_LEN, "%5d.%03d",
      (int32_t)(diff_time / 1000),
      (int32_t)(diff_time % 1000));
  pthread_mutex_unlock(&m_PrintTimeMutex);

  return &m_TimeStr[0];
}
#endif

void InitDefaultValue() {
  FUNCTION_ENTER();

  m_Settings.eCodec = OMX_VIDEO_CodingAVC;  // OMX_VIDEO_CodingH264;
  m_Settings.eLevel = OMX_VIDEO_AVCLevel32;
  m_Settings.eControlRate = OMX_Video_ControlRateVariable;
  m_Settings.eSliceMode = 0;

  m_Settings.nFrameWidth = 1280;
  m_Settings.nFrameHeight = 720;
  m_Settings.nScalingWidth = 0;
  m_Settings.nScalingHeight = 0;
  m_Settings.nRectangleLeft = 0;
  m_Settings.nRectangleTop = 0;
  m_Settings.nRectangleRight = 0;
  m_Settings.nRectangleBottom = 0;
  m_Settings.nFrameBytes = 1280 * 720 * 3 / 2;

  m_Settings.nFramestride = 0;
  m_Settings.nFrameScanlines = 0;
  m_Settings.nFrameRead = 0;

  m_Settings.nBitRate = 17500000;
  m_Settings.nFrameRate = 25;

  // COLOR_FMT_NV12:QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m
  m_Settings.nFormat = QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m;
  m_Settings.nInputBufferCount = 0;
  m_Settings.nInputBufferSize = 0;
  m_Settings.nOutputBufferCount = 0;
  m_Settings.nOutputBufferSize = 0;
  m_Settings.nUserProfile = 0;

  m_Settings.bPrependSPSPPSToIDRFrame = 1;
  m_Settings.bAuDelimiters = 0;
  m_Settings.nIDRPeriod = 1;  // default is 1: every I frame is an IDR
  m_Settings.nPFrames = 47;  // (2 * nFramerate - 1)
  m_Settings.nBFrames = 0;   // TODO: skip it ????
  m_Settings.eResyncMarkerType = 0;  // for H263, RESYNC_MARKER_NONE
  m_Settings.nResyncMarkerSpacing = 0;  // for H263
  m_Settings.nHECInterval = 0;  // for H263

  m_Settings.nRefreshMode = OMX_VIDEO_IntraRefreshCyclic;
  m_Settings.nIntraRefresh = 5;  // refer to nCirMBs

  m_Settings.configFileName = NULL;

  m_Settings.nRotation = 0;
  m_Settings.nFrames = 120;
  m_Settings.nMirror = 0;

  m_TestMode = MODE_FILE_ENCODE;

  pthread_mutex_init(&m_RunningMutex, NULL);
  pthread_cond_init(&m_RunningSignal, NULL);

  FUNCTION_EXIT();
}

void Help() {
  printf("\n\n");
  printf("=============================\n");
  printf("mm-venc-omx-test args... \n");
  printf("=============================\n\n");
  printf("      -m mode (profile, file)\n");
  printf("      -t encode type (mpeg4, h263, h264, vp8, hevc)\n");
  printf("      -w width\n");
  printf("      -h height\n");
  printf("      -f fps\n");
  printf("      -b bitrate\n");
  printf("      -n number of frames to encode\n");
  printf("      -i infile\n");
  printf("      -o outfile\n");
  printf("      -r rotation (90, 180, 270)\n");
  printf("      -d debug level control\n");
  printf("         (Values 0(Error), 1(Performance), 2(Debug), 4(Verbose), 8(Function Enter/Exit)\n");
  printf("      -p IDR-period\n");
  printf("      -c colorformat (NV12, NV12_UBWC, P010)\n");
  printf("      -L rectangle-left\n");
  printf("      -T rectangle-top\n");
  printf("      -R rectangle-right\n");
  printf("      -B rectangle-bottom\n");
  printf("      -j config file\n");
  printf("      -J dynamic config file\n");
  printf("      -M Memory-mode (gbm, ion)\n");
  printf("      --fpscontrol\n");
  printf("          (Valus 0(close), 1(open)) \n");
  printf("      --ratecontrol\n");
  printf("          (Valus 0(disable), 1(variable), 2(constant), 3(variable-skipframes, 4(constant-skipframes), 5(constant-quality)))\n");
  printf("      --scaling-width\n");
  printf("      --scaling-higth\n");
  printf("      --mirror\n");
  printf("      (Values 0(None), 1(Vertical), 2(Horizontal), 3(Both))\n");
  printf("      --refresh-mode (IR_CYCLIC, IR_RANDOM, IR_ADAPTIVE)\n");
  printf("      --refresh-count\n");
  printf("      --resyncmarker-type (MB, BYTE, GOB)\n");
  printf("      --resyncmarker-spacing\n");
  printf("      --metadata-mode \n");
  printf("      (Valus 0(close), 1(open)) \n");
  printf("      --help  Print this menu\n");
  printf("=============================\n\n\n");
}

uint32_t tmp = 0;
static int ParseArgs(int argc, char **argv) {
  int command;
  int rc_opt = 0;
  bool found = false;

  struct option longopts[] = {
    {"mode", required_argument, NULL, 'm'},
    {"type", required_argument, NULL, 't'},
    {"width", required_argument, NULL, 'w'},
    {"height", required_argument, NULL, 'h'},
    {"fps", required_argument, NULL, 'f'},
    {"bitrate", required_argument, NULL, 'b'},
    {"colorformat", required_argument, NULL, 'c'},
    {"infile", required_argument, NULL, 'i'},
    {"outfile", required_argument, NULL, 'o'},
    {"rotation", required_argument, NULL, 'r'},
    {"IDR-period", required_argument, NULL, 'p'},
    {"debug-level", required_argument, NULL, 'd'},
    {"rectangle-left", required_argument, NULL, 'L'},
    {"rectangle-top", required_argument, NULL, 'T'},
    {"rectangle-right", required_argument, NULL, 'R'},
    {"rectangle-bottom", required_argument, NULL, 'B'},
    {"Dynamic-config", required_argument, NULL, 'J'},
    {"Memory-mode", required_argument, NULL, 'M'},
    {"help", no_argument, NULL, 0},
    {"scaling-width", required_argument, NULL, 1},
    {"scaling-height", required_argument, NULL, 2},
    {"fpscontrol", required_argument, NULL, 3},
    {"ratecontrol", required_argument, NULL, 4},
    {"mirror", required_argument, NULL, 5},
    {"refresh-mode", required_argument, NULL, 6},
    {"refresh-count", required_argument, NULL, 7},
    {"resyncmarker-type", required_argument, NULL, 8},
    {"resyncmarker-spacing", required_argument, NULL, 9},
    {"metadata-mode", required_argument, NULL, 10},
    {NULL, 0, NULL, 0}};

  while ((command = getopt_long(argc, argv, "m:t:w:h:f:b:n:c:i:o:r:p:d:s:j:L:T:R:B:J:M:", longopts,
          NULL)) != -1) {
    switch (command) {
      case 'm':
        if (!strcmp("file", optarg)) {
          m_TestMode = MODE_FILE_ENCODE;
        } else if (!strcmp("profile", optarg)) {
          m_TestMode = MODE_PROFILE;
        } else {
          VLOGE("Invalid -m parameter: %s", optarg);
          return -1;
        }
        break;
      case 't':
        m_Settings.eCodec = FindCodecTypeByName(optarg);
        if (m_Settings.eCodec < 0) {
          VLOGE("invalid -t :%s ret: %d",
              optarg, m_Settings.eCodec);
          return -1;
        }
        break;
      case 'w':
        m_Settings.nFrameWidth = atoi(optarg);
        break;
      case 'h':
        m_Settings.nFrameHeight = atoi(optarg);
        break;
      case 'f':
        m_Settings.nFrameRate = atoi(optarg);
        break;
      case 'b':
        m_Settings.nBitRate = atoi(optarg);
        break;
      case 'c':
        m_Settings.nFormat = FindOmxFmtByColorName(optarg);
        break;
      case 'i':
        strlcpy(&m_InputFileName[0], optarg, MAX_FILE_PATH);
        break;
      case 'j':
        m_ConfigFilePath = optarg;
        break;
      case 'J':
        m_DynamicConfigFilePath = optarg;
        break;
      case 'o':
        strlcpy(&m_OutputFileName[0], optarg, MAX_FILE_PATH);
        break;
      case 'r':
        m_Settings.nRotation = atoi(optarg);
        if (m_Settings.nRotation != 90 &&
            m_Settings.nRotation != 180 &&
            m_Settings.nRotation != 270) {
          VLOGE("Invalid -r parmater: rotation: %d", m_Settings.nRotation);
          return -1;
        }
        break;
      case 'p':
        m_Settings.nIDRPeriod = atoi(optarg);
        break;
      case 'd':
        m_DebugLevelSets = atoi(optarg);
        break;
      case 'L':
        m_Settings.nRectangleLeft = atoi(optarg);
        break;
      case 'T':
        m_Settings.nRectangleTop = atoi(optarg);
        break;
      case 'R':
        m_Settings.nRectangleRight = atoi(optarg);
        break;
      case 'B':
        m_Settings.nRectangleBottom = atoi(optarg);
        break;
      case 'M':
        m_MemoryMode = optarg;
        break;
      case 0:
        Help();
        exit(0);
      case 1:
        m_Settings.nScalingWidth = atoi(optarg);
        break;
      case 2:
        m_Settings.nScalingHeight = atoi(optarg);
        break;
      case 3:
        m_ControlFps = atoi(optarg);
        break;
      case 4:
        m_Settings.eControlRate = (OMX_VIDEO_CONTROLRATETYPE)(atoi(optarg));
        break;
      case 5:
        m_Settings.nMirror = atoi(optarg);
        break;
      case 6:
        m_Settings.nRefreshMode = FindIntraRefreshModeByName(optarg);
        break;
      case 7:
        m_Settings.nIntraRefresh = atoi(optarg);
        break;
      case 8:
        m_Settings.eResyncMarkerType = FindResyncMarkerTypeByName(optarg);
        break;
      case 9:
        m_Settings.nResyncMarkerSpacing = atoi(optarg);
        break;
      case 10:
        m_MetadataMode = atoi(optarg);
        break;
      default:
        return -1;
    }
  }

  if (m_Settings.nFrameRate <= 0) {
    VLOGE("Invalid input frame rate");
    return -1;
  }

  uint32_t crop_width = m_Settings.nRectangleRight - m_Settings.nRectangleLeft;
  uint32_t crop_height = m_Settings.nRectangleBottom - m_Settings.nRectangleTop;
  if (crop_width != 0 && crop_height != 0) {
    if (crop_width % 512 != 0 && crop_height % 512 != 0) {
      VLOGE("Invalid crop frame parameter, crop width/height need 512 align!");
      return -1;
    }
  }

  return 0;
}


int ReadFile(const char* filename, char** content) {
  // open in read binary mode
  FILE* file = fopen(filename, "rb");
  if (file == NULL) {
    VLOGE("read file fail: %s", filename);
    return -1;
  }

  // get the length
  fseek(file, 0, SEEK_END);
  int64_t length = ftell(file);
  fseek(file, 0, SEEK_SET);

  // allocate content buffer
  *content = reinterpret_cast<char*>(malloc((size_t)length + sizeof("")));
  if (*content == NULL) {
    VLOGE("malloc failed");
    return -1;
  }

  // read the file into memory
  size_t read_chars = fread(*content, sizeof(char), (size_t)length, file);
  if ((int64_t)read_chars != length) {
    VLOGE("length dismatch: %d, %d", read_chars, length);
    free(*content);
    return -1;
  }
  (*content)[read_chars] = '\0';

  fclose(file);
  return 0;
}

int ParseConfigs(char * filename) {
  FUNCTION_ENTER();

  if (filename == NULL) {
    VLOGE("Invalid parameter");
    FUNCTION_EXIT();
    return -1;
  }

  char * json_string = NULL;
  if (ReadFile(filename, &filename) != 0) {
    VLOGE("read file failed");
    FUNCTION_EXIT();
    return -1;
  }

  if ((json_string == NULL) || (json_string[0] == '\0') || (json_string[1] == '\0')) {
    VLOGE("file content is null");
    return -1;
  }

  JsonItem *root = GetRoot(json_string);
  JsonItem *object = GetItem(root, "profile");
  JsonItem *item;
  item = GetItem(object, "InFile");
  if (item != NULL) {
    strlcpy(&m_InputFileName[0], item->valuestring, MAX_FILE_PATH);
  }
  item = GetItem(object, "OutFile");
  if (item != NULL) {
    strlcpy(&m_OutputFileName[0], item->valuestring, MAX_FILE_PATH);
  }
  item = GetItem(object, "Type");
  if (item != NULL) {
    m_Settings.eCodec = item->valueint;
  }
  item = GetItem(object, "Width");
  if (item != NULL) {
    m_Settings.nFrameWidth = item->valueint;
  }
  item = GetItem(object, "Height");
  if (item != NULL) {
    m_Settings.nFrameHeight = item->valueint;
  }
  item = GetItem(object, "Scaling-Width");
  if (item != NULL) {
    m_Settings.nScalingWidth = item->valueint;
  }
  item = GetItem(object, "Scaling-Height");
  if (item != NULL) {
    m_Settings.nScalingHeight = item->valueint;
  }
  item = GetItem(object, "Fps");
  if (item != NULL) {
    m_Settings.nFrameRate = item->valueint;
  }
  item = GetItem(object, "Bitrate");
  if (item != NULL) {
    m_Settings.nBitRate = item->valueint;
  }
  item = GetItem(object, "RateControl");
  if (item != NULL) {
    m_Settings.eControlRate = item->valueint;
  }
  item = GetItem(object, "Rotation");
  if (item != NULL) {
    m_Settings.nRotation = item->valueint;
  }
  item = GetItem(object, "IDR-Period");
  if (item != NULL) {
    m_Settings.nIDRPeriod = item->valueint;
  }
  item = GetItem(object, "Debug-Level");
  if (item != NULL) {
    m_DebugLevelSets = item->valueint;
  }
  item = GetItem(object, "Control-Fps");
  if (item != NULL) {
    m_ControlFps = item->valueint;
  }
  item = GetItem(object, "Color-Formate");
  if (item != NULL) {
    m_Settings.nFormat = FindOmxFmtByColorName(item->valuestring);
  }
  item = GetItem(object, "Mirror");
  if (item != NULL) {
    m_Settings.nMirror = item->valueint;
  }
  item = GetItem(object, "Rectangle-Left");
  if (item != NULL) {
    m_Settings.nRectangleLeft = item->valueint;
  }
  item = GetItem(object, "Rectangle-Top");
  if (item != NULL) {
    m_Settings.nRectangleTop = item->valueint;
  }
  item = GetItem(object, "Rectangle-Right");
  if (item != NULL) {
    m_Settings.nRectangleRight = item->valueint;
  }
  item = GetItem(object, "Rectangle-Bottom");
  if (item != NULL) {
    m_Settings.nRectangleBottom = item->valueint;
  }
  item = GetItem(object, "DynamicFile");
  if (item != NULL) {
    m_DynamicConfigFilePath = item->valuestring;
  }
  item = GetItem(object, "IntraRefresh-Mode");
  if (item != NULL) {
    m_Settings.nRefreshMode = FindIntraRefreshModeByName(item->valuestring);
  }
  item = GetItem(object, "IntraRefresh-Count");
  if (item != NULL) {
    m_Settings.nIntraRefresh = item->valueint;
  }
  item = GetItem(object, "ResyncMarker-Type");
  if (item != NULL) {
    m_Settings.eResyncMarkerType = FindResyncMarkerTypeByName(item->valuestring);
  }
  item = GetItem(object, "ResyncMarker-Spacing");
  if (item != NULL) {
    m_Settings.nResyncMarkerSpacing = item->valueint;
  }
  item = GetItem(object, "Metadata-Mode");
  if (item != NULL) {
    m_MetadataMode = item->valueint;
  }
  item = GetItem(object, "Memory-Mode");
  if (item != NULL) {
    strlcpy(m_MemoryMode, item->valuestring, MAX_FILE_PATH);
  }

  FUNCTION_EXIT();
  return 0;
}

int ParseDynamicConfigs(char * filename) {
  FUNCTION_ENTER();

  if (filename == NULL) {
    VLOGE("Invalid parameter");
    FUNCTION_EXIT();
    return -1;
  }

  char * json_string = NULL;
  if (ReadFile(filename, &json_string) != 0) {
    VLOGE("read file failed");
    FUNCTION_EXIT();
    return -1;
  }

  if ((json_string == NULL) || (json_string[0] == '\0') || (json_string[1] == '\0')) {
    VLOGE("file content is null");
    return -1;
  }

  JsonItem *root = GetRoot(json_string);
  JsonItem *list = GetItem(root, "plist");
  if (list != NULL) {
    mDynamicConfigNum = GetArraySize(list);
    VLOGD("    mDynamicConfigNum : %d", mDynamicConfigNum);
    m_DynamicConfigures = new DynamicConfigure[4];
    for (int i = 0; i < mDynamicConfigNum; ++i) {
      JsonItem *profile = GetArrayItem(list, i);
      JsonItem *item = GetItem(profile, "frame_num");
      if (item != NULL) {
        m_DynamicConfigures[i].frame_num = item->valueint;
      }
      item = GetItem(profile, "config_param");
      if (item != NULL) {
        m_DynamicConfigures[i].config_param = (OMX_INDEXTYPE)(item->valueint);
      }
      if (m_DynamicConfigures[i].config_param == OMX_IndexConfigVideoBitrate) {
        JsonItem *item = GetItem(profile, "bitrate");
        if (item != NULL) {
          m_DynamicConfigures[i].config_data.bitrate.nEncodeBitrate = (OMX_U32)(item->valueint);
          m_DynamicConfigures[i].config_data.bitrate.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
        }
      } else if (m_DynamicConfigures[i].config_param == OMX_IndexConfigVideoFramerate) {
        JsonItem *item = GetItem(profile, "framerate");
        if (item != NULL) {
          m_DynamicConfigures[i].config_data.framerate.xEncodeFramerate = (item->valueint) >> 16;
          m_DynamicConfigures[i].config_data.framerate.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
        }
      } else if (m_DynamicConfigures[i].config_param == OMX_IndexConfigVideoAVCIntraPeriod) {
        JsonItem *item;
        item = GetItem(profile, "BFrame");
        if (item != NULL) {
          m_DynamicConfigures[i].config_data.intraperiod.nBFrames = (OMX_U32)(item->valueint);
        }
        item = GetItem(profile, "pFrame");
        if (item != NULL) {
          m_DynamicConfigures[i].config_data.intraperiod.nPFrames = (OMX_U32)(item->valueint);
        }
        item = GetItem(profile, "IDRPeriod");
        if (item != NULL) {
          m_DynamicConfigures[i].config_data.intraperiod.nIDRPeriod = (OMX_U32)(item->valueint);
        }
        m_DynamicConfigures[i].config_data.framerate.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
      }
    }
  }

  FUNCTION_EXIT();
  return 0;
}

int ProcessDynamicConfiguration(uint32_t frame_num) {
  FUNCTION_ENTER();

  int result = 0;
  if (m_DynamicConfigures != NULL) {
    for (int i = 0; i < mDynamicConfigNum; i++) {
      VLOGD("m_DynamicConfigures-frame_num:%d", m_DynamicConfigures[i].frame_num);
      if (frame_num == m_DynamicConfigures[i].frame_num) {
        SetDynamicConfig(m_DynamicConfigures[i].config_param, &m_DynamicConfigures[i].config_data);
      }
    }
  }
  FUNCTION_EXIT();
  return true;
}

void DumpSetting() {
  const char * codec = FindCodecNameByType(m_Settings.eCodec);
  VLOGD("============Dump user settings===========");
  if (codec != NULL) {
    VLOGD("eCodec: %d(%s), eLevel: %d, eControlRate: %d, eSliceMode: %d",
        m_Settings.eCodec, codec,
        m_Settings.eLevel,
        m_Settings.eControlRate,
        m_Settings.eSliceMode);
  } else {
    VLOGD("Invalid codec Type! ");
  }

  VLOGD("Input Frame w/h: %dx%d",
      m_Settings.nFrameWidth,
      m_Settings.nFrameHeight);

  VLOGD("stride: %d, scanlines: %d, frame num:%d",
      m_Settings.nFramestride,
      m_Settings.nFrameScanlines,
      m_Settings.nFrameRead);

  VLOGD("BitRate: %d, frame rate: %d, format: 0x%x",
      m_Settings.nBitRate,
      m_Settings.nFrameRate,
      m_Settings.nFormat);
  VLOGD("Component inport buffer count: %d, size: %d",
      m_Settings.nInputBufferCount,
      m_Settings.nInputBufferSize);
  VLOGD("Component outport buffer count: %d, size: %d",
      m_Settings.nOutputBufferCount,
      m_Settings.nOutputBufferSize);
  VLOGD("user profile: %d", m_Settings.nUserProfile);
  VLOGD("prepend SPS PPS to IDR frame: %d(true/false)",
      m_Settings.bPrependSPSPPSToIDRFrame);
  VLOGD("Au delimiters: %d (true/false)",
      m_Settings.bAuDelimiters);
  VLOGD("IDR period: %d, P Frame: %d, B Frame: %d",
      m_Settings.nIDRPeriod,
      m_Settings.nPFrames,
      m_Settings.nBFrames);
  VLOGD("Resync marker type: %d, spacing: %d, HEC interval: %d",
      m_Settings.eResyncMarkerType,
      m_Settings.nResyncMarkerSpacing,
      m_Settings.nHECInterval);
  VLOGD("Refresh mode: %d, intra refresh: %d",
      m_Settings.nRefreshMode,
      m_Settings.nIntraRefresh);
  VLOGD("Configure file name: %s", m_Settings.configFileName);
  VLOGD("Rotation: %d, Frame num: %d",
      m_Settings.nRotation,
      m_Settings.nFrames);
  VLOGD("Input File: %s", &m_InputFileName[0]);
  VLOGD("Output File: %s", &m_OutputFileName[0]);

  VLOGD("=========================================\n\n");
}

int GetFileSize() {
  FILE * fp = fopen(m_InputFileName, "r");
  int size = 0;
  if (fp == NULL) {
    VLOGE("InputFile open failed.");
    return size;
  }
  fseek(fp, 0L, SEEK_END);
  size = ftell(fp);
  fclose(fp);
  return size;
}

bool GetNextFrameForPerformanceTest(unsigned char * p_buffer,
    int32_t *readLen) {
  FUNCTION_ENTER();

  if (p_buffer == NULL || readLen == NULL) {
    VLOGE("bad parameters: pBuffer: %p, readLen: %p", p_buffer, readLen);
    return false;
  }

  int ret = GetNextFrameFromBuffer(p_buffer, readLen, m_EncodeFrameNum);

  FUNCTION_EXIT();
  return ret;
}

void ProcessInputCropMetadata(unsigned char * p_buffer, unsigned long color_format) {
  FUNCTION_ENTER();
  if (!m_Settings.nRectangleLeft && !m_Settings.nRectangleTop
      && !m_Settings.nRectangleRight && !m_Settings.nRectangleBottom) {
    VLOGD("skip crop!");
    return;
  }

  unsigned int lstride, lscanl, cstride, cscanl, y_plane, uv_plane, yuv_size;
  lstride = VENUS_Y_STRIDE(color_format, m_Settings.nFrameWidth);
  lscanl = VENUS_Y_SCANLINES(color_format, m_Settings.nFrameHeight);
  cstride = VENUS_UV_STRIDE(color_format, m_Settings.nFrameWidth);
  cscanl = VENUS_UV_SCANLINES(color_format, m_Settings.nFrameHeight);
  y_plane = lstride * lscanl;
  uv_plane = cstride * cscanl;
  yuv_size = ALIGN(y_plane + uv_plane, 4096);
  VLOGD("yuv size:%d", yuv_size);


  OMX_QCOM_EXTRADATA_FRAMEDIMENSION *framedimension_format;
  OMX_OTHER_EXTRADATATYPE *p_extra;
  p_extra = reinterpret_cast<OMX_OTHER_EXTRADATATYPE *>((unsigned long long)(p_buffer + yuv_size + 3)&(~3));
  p_extra->eType = (OMX_EXTRADATATYPE) OMX_ExtraDataFrameDimension;
  p_extra->nSize = sizeof(OMX_OTHER_EXTRADATATYPE) + sizeof(OMX_QCOM_EXTRADATA_FRAMEDIMENSION);
  framedimension_format = reinterpret_cast<OMX_QCOM_EXTRADATA_FRAMEDIMENSION *>(p_extra->data);
  framedimension_format->nDecWidth = m_Settings.nRectangleLeft;
  framedimension_format->nDecHeight = m_Settings.nRectangleTop;
  framedimension_format->nActualWidth = m_Settings.nRectangleRight - m_Settings.nRectangleLeft;
  framedimension_format->nActualHeight = m_Settings.nRectangleBottom - m_Settings.nRectangleTop;
  VLOGD("nActualWidth: %d", framedimension_format->nActualWidth);
  VLOGD("nActualHeight: %d", framedimension_format->nActualHeight);
  FUNCTION_EXIT();
}

// TODO: currently use the default input format: NV12
//       Need to support more input format
bool GetNextFrameFromFile(unsigned char * p_buffer, int32_t *read_length) {
  bool status = false;
  int32_t read_data_length = 0;

  FUNCTION_ENTER();

  if (p_buffer == NULL || read_length == NULL) {
    VLOGE("bad parameters: p_buffer: %p, read_length: %p", p_buffer, read_length);
    return false;
  }

  // open input file
  if (m_InputFileFd < 0) {
    if (m_InputFileName != NULL) {
      m_InputFileFd = open(m_InputFileName, O_RDONLY);
      if (m_InputFileFd < 0) {
        VLOGE("Can not open file: %s, error: %s",
            m_InputFileName, strerror(errno));
        FUNCTION_EXIT();
        return false;
      }
    } else {
      VLOGE("No input file");
      FUNCTION_EXIT();
      return false;
    }
  }

  //ProcessInputCropMetadata(pBuffer);
  unsigned long lscanl, lstride, cscanl, cstride;
  unsigned long color_format = (m_Settings.eCodec == OMX_VIDEO_CodingImageHEIC ?
      COLOR_FMT_NV12_512 : ConvertColorFomat(m_Settings.nFormat));
  unsigned long i, bytes;
  unsigned char *yuv = p_buffer;
  unsigned int extra_size = 0;

  // two plane: Y, UV, width align
  lstride = VENUS_Y_STRIDE(color_format, m_Settings.nFrameWidth);
  lscanl = VENUS_Y_SCANLINES(color_format, m_Settings.nFrameHeight);
  cstride = VENUS_UV_STRIDE(color_format, m_Settings.nFrameWidth);
  cscanl = VENUS_UV_SCANLINES(color_format, m_Settings.nFrameHeight);

  switch (color_format) {
    case COLOR_FMT_NV12:
    case COLOR_FMT_NV12_512:
      // read plane Y
      for (i = 0; i < m_Settings.nFrameHeight; i++) {
        bytes = read(m_InputFileFd, yuv, m_Settings.nFrameWidth);
        if (bytes != m_Settings.nFrameWidth) {
          VLOGE("read failed: %d != %d, already read: %d error: %s",
              bytes, m_Settings.nFrameWidth, read_data_length, strerror(errno));
          FUNCTION_EXIT();
          return false;
        }
        read_data_length += bytes;
        yuv += lstride;
      }
      yuv = p_buffer + (lscanl * lstride);
      // read plane U/V
      for (i = 0; i < m_Settings.nFrameHeight/2; i++) {
        bytes = read(m_InputFileFd, yuv, m_Settings.nFrameWidth);
        if (bytes != m_Settings.nFrameWidth) {
          VLOGE("read failed: %d != %d, already read: %d error: %s",
              bytes, m_Settings.nFrameWidth, read_data_length, strerror(errno));
          FUNCTION_EXIT();
          return false;
        }
        read_data_length += bytes;
        yuv += cstride;
      }
      *read_length = VENUS_BUFFER_SIZE(color_format, m_Settings.nFrameWidth, m_Settings.nFrameHeight);
      break;
    case COLOR_FMT_NV12_UBWC:
    case COLOR_FMT_NV12_BPP10_UBWC:
      if (!strcmp(m_MemoryMode, "gbm")) {
        m_Settings.nFrameBytes = VENUS_BUFFER_SIZE_USED(color_format,
            m_Settings.nFrameWidth, m_Settings.nFrameHeight, false);
      } else {
        m_Settings.nFrameBytes = VENUS_BUFFER_SIZE(color_format,
            m_Settings.nFrameWidth, m_Settings.nFrameHeight);
      }

      read_data_length = read(m_InputFileFd, p_buffer, m_Settings.nFrameBytes);
      if (read_data_length != m_Settings.nFrameBytes) {
        if (errno == 0 && read_data_length == 0) {
          VLOGE("Input file read to end");
        } else {
          VLOGE("read file error, read: %d, real: %d, error: %s",
              m_Settings.nFrameBytes, read_data_length, strerror(errno));
        }
        FUNCTION_EXIT();
        return false;
      }
      *read_length = read_data_length;
      break;
    case COLOR_FMT_P010:
      // read plane Y
      for (i = 0; i < m_Settings.nFrameHeight; i++) {
        bytes = read(m_InputFileFd, yuv, m_Settings.nFrameWidth * 2);
        if (bytes != m_Settings.nFrameWidth * 2) {
          VLOGE("read failed: %d != %d, already read: %d error: %s",
              bytes, m_Settings.nFrameWidth * 2, read_data_length, strerror(errno));
          FUNCTION_EXIT();
          return false;
        }
        read_data_length += bytes;
        yuv += lstride;
      }
      yuv = p_buffer + (lscanl * lstride);
      // read plane U/V
      for (i = 0; i < m_Settings.nFrameHeight/2; i++) {
        bytes = read(m_InputFileFd, yuv, m_Settings.nFrameWidth * 2);
        if (bytes != m_Settings.nFrameWidth * 2) {
          VLOGE("read failed: %d != %d, already read: %d error: %s",
              bytes, m_Settings.nFrameWidth * 2, read_data_length, strerror(errno));
          FUNCTION_EXIT();
          return false;
        }
        read_data_length += bytes;
        yuv += cstride;
      }

      *read_length = VENUS_BUFFER_SIZE(color_format, m_Settings.nFrameWidth, m_Settings.nFrameHeight);
      break;
    default:
      break;
  }
  VLOGD("Actual read bytes: %d, NV12 buffer size: %d\n\n\n", read_data_length, *read_length);

  ProcessInputCropMetadata(p_buffer, color_format);
  FUNCTION_EXIT();
  return true;
}

/*
 *  used by Ecoder to get input frame
 *
 */
bool GetNextFrame(unsigned char * p_buffer, int32_t *read_length) {
  bool status = false;

  FUNCTION_ENTER();

  if (m_InputDataSize == 0) {
    gettimeofday(&m_EncodeStartTime, NULL);
  }

  switch (m_TestMode) {
    case MODE_FILE_ENCODE:
      status = GetNextFrameFromFile(p_buffer, read_length);
      break;
    case MODE_PROFILE:
      status = GetNextFrameForPerformanceTest(p_buffer, read_length);
      break;
    default:
      VLOGE("bad test mode: %d", m_TestMode);
      status = false;
  }

  VLOGD("readLen: %d", *read_length);
  if (status && *read_length != 0) {
    m_EncodeFrameNum++;
    m_InputDataSize += *read_length;
  }

  FUNCTION_EXIT();
  return status;
}

bool StoreEncodedDataToFile(unsigned char * p_buffer, int32_t data_length, unsigned long long time_stamp) {
  bool status = false;
  int32_t write_data_length = 0;

  FUNCTION_ENTER();

  if (p_buffer == NULL || data_length <= 0) {
    VLOGE("bad parameters: p_buffer: %p, write_length: %d", p_buffer, data_length);
    return false;
  }

  // open input file
  if (m_OutputFileFd < 0) {
    if (m_OutputFileName != NULL) {
      m_OutputFileFd = open(m_OutputFileName, O_WRONLY|O_CREAT,
          S_IRWXU | S_IRWXG | S_IRWXO);  // 0x0777
      if (m_OutputFileFd < 0) {
        VLOGE("Can not open file: %s, error: %s",
            m_OutputFileName, strerror(errno));
        FUNCTION_EXIT();
        return false;
      }
    } else {
      VLOGE("No Output file");
      FUNCTION_EXIT();
      return false;
    }
  }

  if (m_Settings.eCodec == OMX_VIDEO_CodingVP8) {
    write(m_OutputFileFd, &data_length, 4);
    write(m_OutputFileFd, &time_stamp, 8);
  }

  write_data_length = write(m_OutputFileFd, p_buffer, data_length);
  if (write_data_length != data_length) {
    VLOGE("Write buffer failed, write:%d, ret: %d, error: %s",
        data_length, write_data_length, strerror(errno));

    FUNCTION_EXIT();
    return false;
  }

  m_OutputDataSize += data_length;

  FUNCTION_EXIT();
  return true;
}

bool StoreEncodedDataForPerformanceTest(unsigned char * p_buffer,
    int32_t data_length) {
  FUNCTION_ENTER();


  FUNCTION_EXIT();
  return true;
}

/*
 *  used by Ecoder to store output data
 *
 */
bool StoreEncodedData(unsigned char * p_buffer, int32_t data_length, unsigned long long time_stamp) {
  bool status = false;

  FUNCTION_ENTER();

  if (data_length > 0) {
    gettimeofday(&m_EncodeEndTime, NULL);
  }

  switch (m_TestMode) {
    case MODE_FILE_ENCODE:
      status = StoreEncodedDataToFile(p_buffer, data_length, time_stamp);
      break;
    case MODE_PROFILE:
      status = StoreEncodedDataForPerformanceTest(p_buffer, data_length);
      break;
    default:
      VLOGE("bad test mode: %d", m_TestMode);
      status = false;
  }

  FUNCTION_EXIT();
  return status;
}



void NotifyToStop() {
  FUNCTION_ENTER();

  pthread_mutex_lock(&m_RunningMutex);
  m_Running = false;
  pthread_cond_signal(&m_RunningSignal);
  pthread_mutex_unlock(&m_RunningMutex);

  FUNCTION_EXIT();
}

void WaitingForCodec() {
  FUNCTION_ENTER();

  pthread_mutex_lock(&m_RunningMutex);
  if (m_Running == true) {
    pthread_cond_wait(&m_RunningSignal, &m_RunningMutex);
  }
  pthread_mutex_unlock(&m_RunningMutex);

  FUNCTION_EXIT();
}

void ReleaseResource() {
  FUNCTION_ENTER();

  ReleaseCodec();

  if (m_InputFileFd >= 0) {
    close(m_InputFileFd);
    m_InputFileFd = -1;
  }

  if (m_OutputFileFd >= 0) {
    close(m_OutputFileFd);
    m_OutputFileFd = -1;
  }

  pthread_mutex_destroy(&m_RunningMutex);
  pthread_cond_destroy(&m_RunningSignal);

  FUNCTION_EXIT();
}

void PrintStatisticalData() {
  FUNCTION_ENTER();

  m_TotalTime =
    ((m_EncodeEndTime.tv_sec - m_EncodeStartTime.tv_sec) * 1000.0 + \
     (m_EncodeEndTime.tv_usec - m_EncodeStartTime.tv_usec) / 1000) / 1000;

  VLOGP("\n\n=======================Statistical Data=====================");
  VLOGP("Input file name: %s", m_InputFileName);
  VLOGP("Output file name: %s", m_OutputFileName);
  const char * file_format = FindColorNameByOmxFmt(m_Settings.nFormat);
  if (file_format != NULL) {
    VLOGP("Input file format: %s", file_format);
  } else {
    VLOGE("Input file format: null file format");
  }
  const char * codec = FindCodecNameByType(m_Settings.eCodec);
  if (codec != NULL) {
    VLOGP("Codec: %s", codec);
  } else {
    VLOGE("Codec: null codec");
  }
  VLOGP("Encode input data size: %d bytes", m_InputDataSize);
  VLOGP("Encode output data size: %d bytes", m_OutputDataSize);
  VLOGP("Encode Time: %fs", m_TotalTime);
  VLOGP("Encode frame: %d", m_EncodeFrameNum);
  VLOGP("\n===========================================================\n\n");

  FUNCTION_EXIT();
}

void PrintCPUData() {
  FUNCTION_ENTER();

  VLOGP("\n\n=======================CPU Data=====================");
  VLOGP("Occupied physical memory: %d", GetPhysicalMem(m_Pid));
  VLOGP("Total system memory: %d", GetTotalMem());
  VLOGP("CPU time of a process: %llu", GetCpuProcessOccupy(m_Pid));
  VLOGP("Total CPU time: %llu", GetCpuTotalOccupy() - m_CpuOccupyStartTime);
  VLOGP("Process CPU usage: %f%", GetProcessCpu(m_Pid, m_CpuOccupyStartTime));
  VLOGP("Process memory usage: %f%", GetProcessMem(m_Pid));
  VLOGP("\n===========================================================\n\n");

  FUNCTION_EXIT();
}

int main(int argc, char **argv) {
  int status = 0;
  bool ret = false;

  if (argc < 2) {
    printf("===================================\n");
    printf("To use it: ./venc-omx-sample --help\n");
    printf("===================================\n");
    exit(0);
  } else if (argc == 2 && !strcmp(argv[1], "--help")) {
    Help();
    exit(0);
  }

  FUNCTION_ENTER();
  printf("\nVideo encode sample app start...\n");

  pthread_mutex_init(&m_PrintTimeMutex, NULL);
  gettimeofday(&m_StartTime, NULL);

  m_CpuOccupyStartTime = GetCpuTotalOccupy();
  m_Pid = getpid();
  PrintCPUData();

  m_Running = true;

  InitDefaultValue();

  status = ParseArgs(argc, argv);
  CHECK_STATUS("Invalid args!", status);

  if (m_ConfigFilePath != NULL) {
    status = ParseConfigs(m_ConfigFilePath);
    CHECK_STATUS("parse configs file failed", status);
  }

  if (m_DynamicConfigFilePath != NULL) {
    VLOGD("parse dynamic configs file:%s", m_DynamicConfigFilePath);
    status = ParseDynamicConfigs(m_DynamicConfigFilePath);
    CHECK_STATUS("parse dynamic configs file failed", status);
  }

  DumpSetting();

  ret = InitializeCodec(m_Settings.eCodec);
  if (ret == false) {
    VLOGE("Init codec failed, exit");
    ReleaseResource();
    FUNCTION_EXIT();
    return -1;
  }

  ret = ConfigEncoder(&m_Settings);
  if (ret == false) {
    VLOGE("Config codec failed, exit");
    ReleaseResource();
    FUNCTION_EXIT();
    return -1;
  }

  // prepare for performance test
  if (m_TestMode == MODE_PROFILE) {
    status = PrepareForPerformanceTest();
    CHECK_STATUS("performance prepare failed", status);
  }

  ret = StartEncoding();
  if (ret == false) {
    VLOGE("Config codec failed, exit");
    ReleaseResource();
    FUNCTION_EXIT();
    return -1;
  }

  WaitingForCodec();
  StopEncoding();

  ReleaseResource();
  PrintCPUData();
  PrintStatisticalData();
  PrintDynamicalData();

  printf("\nVideo encode sample app finished...\n");

  FUNCTION_EXIT();
  return 0;
}
