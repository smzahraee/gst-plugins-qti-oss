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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vidc/media/msm_media_info.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include "video_test_debug.h"
#include "video_decode_sample.h"
#include "omx_utils.h"
#include "cpu_utils.h"
#include "OMX_Core.h"
#include "OMX_Component.h"
#include "OMX_QCOMExtns.h"

#define CHECK_STATUS(str, result) if (result != 0) \
                            { \
                              VLOGE("%s", str); \
                              FUNCTION_EXIT(); \
                              return -1; \
                            }

// DEBUG log level: LVL_P | LVL_D | LVL_V | LVL_F
uint32_t debug_level_sets = LVL_P;
VideoCodecSetting_t m_Settings;
// refer to enum Mode
int32_t m_TestMode = 1; //default File mode

// FPS control mode
int32_t m_FpsControl = 0;

// used to sync multi-thread print log time: avoid reentrant GetTimeStamp function
static pthread_mutex_t m_PrintTimeMutex;
struct timeval m_StartTime;  // used for print log time

#ifdef PRINT_TIME
#include <sys/time.h>

#define MAX_STR_LEN 32
static char m_TimeStr[MAX_STR_LEN] = {" "};

int m_Pid = 0;

// format the time diff between main function start to log output
// for example: 0.123, 1.456
// please take attention about m_StartTime
char * GetTimeStamp()
{
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

void Help()
{
  printf("\n\n");
  printf("=============================\n");
  printf("vdec-omx-sample args... \n");
  printf("=============================\n\n");
  printf("      -m mode (profile, file)\n");
  printf("      -t file type \n");
  printf("           (Values 1 (PER ACCESS UNIT CLIP (.dat). Clip only available for H264 and Mpeg4)),\n");
  printf("           (Values 2 (ARBITRARY_BYTES (need .264/.265/.m2v)))\n");
  printf("         For H264:\n");
  printf("           (Values 4 (START CODE BASED CLIP (.264/.h264)))\n");
  printf("         For HEVC/HEIC:\n");
  printf("           (Values 4 (START CODE BASED CLIP (.265/.h265)))\n");
  printf("         For MP4/H263:\n");
  printf("           (Values 3 (MP4 VOP or H263 P0 SHORT HEADER START CODE CLIP (.m4v or .263)))\n");
  printf("         For WMV:\n");
  printf("           (Values 3 (VC1 clip Simple/Main Profile (.rcv)))\n");
  printf("           (Values 4 (VC1 clip Advance Profile (.vc1)))\n");
#if 0
  printf("         For DIVX:\n");
  printf("           (Values 3 (DivX 4, 5, 6 clip (.cmp)))\n");
  printf("           (Values 4 (DivX 3.11 clip (.cmp)))\n");
#endif
  printf("         For MPEG2:\n");
  printf("           (Values 3 (MPEG2 START CODE CLIP (.m2v)))\n");
  printf("         For VP8/VP9:\n");
  printf("           (Values 4 (VP8/VP9 START CODE CLIP (.ivf)))\n");
  printf("      -e decode format (MPEG2, MPEG4, H263, H264, HEVC, HEIC, WMV, VP8, VP9)\n");
  printf("      -f fps\n");
  printf("      -n number of frames to decode\n");
  printf("      -w FrameWidth\n");
  printf("      -h FrameHeight\n");
  printf("      -i infile\n");
  printf("      -o outfile\n");
  printf("      -d debug level control\n");
  printf("         (Values 0(Error), 1(Performance), 2(Debug), 4(Verbose), 8(Function Enter/Exit)\n");
  printf("      -p PictureOrder 0(Display order), 1(Decode order)\n");
  printf("      -c colorformat (NV12, NV12_UBWC, TP10_UBWC)\n");
  printf("      -s NalSize (2, 4)\n");
  printf("      --help show help menu\n");
}

static int ParseArgs(int argc, char **argv)
{
  int command;
  for (int i = 0; i < argc; i++)
  {
    VLOGD("arg No.%d: %s", i, argv[i]);
  }
  if (argc < 2)
  {
    printf("===================================\n");
    printf("To use it: ./vdec-omx-sample --help\n");
    printf("===================================\n");
    exit(0);
  }
  struct option longopts[] = {
    {"mode", required_argument, NULL, 'm'},
    {"type", required_argument, NULL, 't'},
    {"codecformat", required_argument, NULL, 'e'}, // d
    {"fps", required_argument, NULL, 'f'},
    {"fpscontrol", required_argument, NULL, 'C'},
    {"width", required_argument, NULL, 'w'},
    {"height", required_argument, NULL, 'h'},
    {"colorformat", required_argument, NULL, 'c'},
    {"pictureorder", required_argument, NULL, 'p'},
    {"nalsize", required_argument, NULL, 's'},
    {"infile", required_argument, NULL, 'i'},
    {"outfile", required_argument, NULL, 'o'},
    {"debug-level", required_argument, NULL, 'd'}, // l
    {"help", no_argument, NULL, 0},
    {NULL, 0, NULL, 0}};
  while ((command = getopt_long(argc, argv, "m:t:e:f:C:n:w:h:c:p:s:i:o:d:L:T:R:B:", longopts,
          NULL)) != -1)
  {
    switch (command)
    {
      case 'm':
        if (!strcmp("file", optarg))
        {
          m_TestMode = MODE_FILE_DECODE;
        }
        else if (!strcmp("profile", optarg))
        {
          m_TestMode = MODE_PROFILE;
        }
        else
        {
          VLOGE("Invalid -m parameter: %s", optarg);
          return -1;
        }
        break;
      case 't':
        m_Settings.nFileType = atoi(optarg);
        break;
      case 'e':
        m_Settings.sCodecName = optarg;
        m_Settings.eCodec = FindCodecTypeByName(optarg);
        if (m_Settings.eCodec < 0)
        {
          VLOGE("invalid -t :%s ret: %d",
              optarg, m_Settings.eCodec);
          return -1;
        }
        break;
      case 'f':
        m_Settings.nFrameRate = atoi(optarg);
        //m_TimeStampInterval = 1e6 / m_Settings.nFrameRate;
        m_Settings.nTimestampInterval = 1e6 / m_Settings.nFrameRate;
        break;
      case 'C':
        m_FpsControl = atoi(optarg);
        break;
      case 'w':
        m_Settings.nFrameWidth = atoi(optarg);
        break;
      case 'h':
        m_Settings.nFrameHeight = atoi(optarg);
        break;
      case 'c':
        m_Settings.sColorName = optarg;
        m_Settings.nColorFormat = FindOmxFmtByColorName(optarg);
        break;
      case 'p':
        m_Settings.nPictureOrder = atoi(optarg);
        break;
      case 's':
        m_Settings.nNalSize = atoi(optarg);
        break;
      case 'i':
        strlcpy(&m_InputFileName[0], optarg, MAX_FILE_PATH);
        break;
      case 'o':
        strlcpy(&m_OutputFileName[0], optarg, MAX_FILE_PATH);
        break;
      case 'd':
        debug_level_sets = atoi(optarg);
        break;
      case 0:
        Help();
        exit(0);
      default:
        return -1;
    }
  }

  if (m_Settings.nFrameRate <= 0)
  {
    VLOGE("Invalid input frame rate");
    return -1;
  }

  return 0;
}


void InitDefaultValue()
{
  FUNCTION_ENTER();

  m_Settings.eCodec = OMX_VIDEO_CodingAVC;  // OMX_VIDEO_CodingH264;
  m_Settings.sCodecName = "H264";  // OMX_VIDEO_CodingH264;
  m_Settings.nFileType = 0;
  m_Settings.nPictureOrder = 1; //Decoder order
  m_Settings.nNalSize = 0;
  //m_Settings.eLevel = OMX_VIDEO_AVCLevel32;
  //m_Settings.eControlRate = OMX_Video_ControlRateVariable;
  //m_Settings.eSliceMode = 0;

  m_Settings.nFrameWidth = 1280;
  m_Settings.nFrameHeight = 720;
  //m_Settings.nScalingWidth = 0;
  //m_Settings.nScalingHeight = 0;
  m_Settings.nRectangleLeft = 0;
  m_Settings.nRectangleTop = 0;
  m_Settings.nRectangleRight = 0;
  m_Settings.nRectangleBottom = 0;
  //m_Settings.nFrameBytes = 1280 * 720 * 3 / 2;

  //m_Settings.nFramestride = 0;
  //m_Settings.nFrameScanlines = 0;
  //m_Settings.nFrameRead = 0;

  //m_Settings.nBitRate = 17500000;
  m_Settings.nFrameRate = 30;
  m_Settings.nTimestampInterval = 1000000 / m_Settings.nFrameRate;

  // COLOR_FMT_NV12:QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m
  m_Settings.nColorFormat = QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m;
  m_Settings.sColorName = "NV12";
  //m_Settings.nInputBufferCount = 0;
  //m_Settings.nInputBufferSize = 0;
  //m_Settings.nOutputBufferCount = 0;
  //m_Settings.nOutputBufferSize = 0;
  //m_Settings.nUserProfile = 0;

  //m_Settings.bPrependSPSPPSToIDRFrame = 0;
  //m_Settings.bAuDelimiters = 0;
  //m_Settings.nIDRPeriod = 1;  // default is 1: every I frame is an IDR
  //m_Settings.nPFrames = 47;  // (2 * nFramerate - 1)
  //m_Settings.nBFrames = 0;   // TODO: skip it ????
  //m_Settings.eResyncMarkerType = 0;  // for H263, RESYNC_MARKER_NONE
  //m_Settings.nResyncMarkerSpacing = 0;  // for H263
  //m_Settings.nHECInterval = 0;  // for H263

  //m_Settings.nRefreshMode = OMX_VIDEO_IntraRefreshCyclic;
  //m_Settings.nIntraRefresh = 5;  // refer to nCirMBs

  //m_Settings.configFileName = NULL;

  //m_Settings.nRotation = 0;
  //m_Settings.nFrames = 120;
  //m_Settings.nMirror = 0;

  //m_TestMode = MODE_FILE_DECODE;

  FUNCTION_EXIT();
}

bool CheckAvailableFileType(int32_t fileType)
{
  switch(fileType)
  {
    case FILE_TYPE_DAT_PER_AU:
    case FILE_TYPE_ARBITRARY_BYTES:
    case FILE_TYPE_264_START_CODE_BASED:
    case FILE_TYPE_264_NAL_SIZE_LENGTH:
    case FILE_TYPE_PICTURE_START_CODE:
    case FILE_TYPE_MPEG2_START_CODE:
    case FILE_TYPE_RCV:
    case FILE_TYPE_VC1:
    case FILE_TYPE_VP8:
    case FILE_TYPE_DIVX_4_5_6:
    case FILE_TYPE_DIVX_311:
    case FILE_TYPE_265_START_CODE_BASED:
      return true;
    default:
      return false;
  }
}

void PrintStatisticalData() {
  FUNCTION_ENTER();
  float total_time =
    ((m_DecodeEndTime.tv_sec - m_DecodeStartTime.tv_sec) * 1000.0 + \
           (m_DecodeEndTime.tv_usec - m_DecodeStartTime.tv_usec) / 1000) / 1000;
  const char* color_name = FindColorNameByOmxFmt(m_Settings.nColorFormat);
  const char* codec_name = FindCodecNameByType(m_Settings.eCodec);
  VLOGP("\n\n=======================Statistical Data=====================");
  VLOGP("Input file name: %s", m_InputFileName);
  VLOGP("Output file name: %s", m_OutputFileName);
  if (color_name != NULL) {
    VLOGP("Output file format: %s", color_name);
  } else {
    VLOGP("Output file format: Invalid color format");
  }
  if (codec_name != NULL) {
    VLOGP("Codec: %s", codec_name);
  } else {
    VLOGP("Codec: Invalid codec");
  }
  VLOGP("Decode input data size: %d bytes", m_InputDataSize);
  VLOGP("Decode output data size: %d bytes", m_OutputDataSize);
  VLOGP("Decode Time: %fs", total_time);
  VLOGP("Decode frame: %d", m_InputFrameNum);
  VLOGP("\n===========================================================\n\n");
  FUNCTION_EXIT();
}

void PrintDynamicalData() {
  FUNCTION_ENTER();
  VLOGP("\n\n=======================Dynamical Data=====================");
  VLOGP("Decode Frame Time avg: %5dms", m_DecodeTotalTimeActal / m_InputFrameNum);
  VLOGP("Decode Frame Time Min: %5dms", m_DecodeFrameTimeMin);
  VLOGP("Decode Frame Time Max: %5dms", m_DecodeFrameTimeMax);
  VLOGP("\n===========================================================\n\n");
  FUNCTION_EXIT();
}

void PrintCPUData() {
  FUNCTION_ENTER();

  VLOGP("\n\n=======================CPU Data =====================");
  VLOGP("Occupied physical memory: %d", GetPhysicalMem(m_Pid));
  VLOGP("Total system memory: %d", GetTotalMem());
  VLOGP("CPU time of a process: %d", GetCpuProcessOccupy(m_Pid));
  VLOGP("Total CPU time: %d", GetCpuTotalOccupy());
  VLOGP("Process CPU usage: %f", GetProcessCpu(m_Pid));
  VLOGP("Process memory usage: %f", GetProcessMem(m_Pid));
  VLOGP("\n===========================================================\n\n");

  FUNCTION_EXIT();
}

int main(int argc, char **argv)
{

  int status = 0;
  bool result = true;

  FUNCTION_ENTER();
  VLOGD("Video decoder sample app start...");

  InitDefaultValue();

  status = ParseArgs(argc, argv);
  CHECK_STATUS("Invalid args!", status);

  if (m_Settings.nFileType >= FILE_TYPE_COMMON_CODEC_MAX)
  {
    m_Settings.nFileType = UpdateFileType(m_Settings.nFileType, m_Settings.eCodec);
  }

  result = SetReadBufferType(m_Settings.nFileType, m_Settings.eCodec);
  if (result == false)
  {
    VLOGE("No match file type fpr buffer to read.");
    FUNCTION_EXIT();
    return -1;
  }

  if (!CheckAvailableFileType(m_Settings.nFileType))
  {
    VLOGE("Error: Invalid file type...%d", m_Settings.nFileType);
    FUNCTION_EXIT();
    return -1;
  }

  result = InitializeCodec(m_Settings.eCodec, m_Settings.nFileType);
  if (result == false)
  {
    VLOGE("Error: Decoder Init failed.");
    ReleaseResource();
    FUNCTION_EXIT();
    return -1;
  }

  result = ConfigureCodec(&m_Settings);
  if (result == false)
  {
    VLOGE("Error: Decoder configure failed.");
    ReleaseResource();
    FUNCTION_EXIT();
    return -1;
  }

  result = StartDecoder();
  if (result == false)
  {
    VLOGE("Error: Decoder start failed.");
    ReleaseResource();
    FUNCTION_EXIT();
    return -1;
  }

  WaitingForTestDone();
  StopDecoder();
  ReleaseResource();

  m_Pid = getpid();
  PrintStatisticalData();
  PrintDynamicalData();
  PrintCPUData();

  FUNCTION_EXIT();
  return 0;
}


