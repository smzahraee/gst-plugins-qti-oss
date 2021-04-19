/*--------------------------------------------------------------------------
Copyright (c) 2010 - 2013, 2016 - 2018, The Linux Foundation. All rights reserved.

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

 * Copyright (c) 2011 Benjamin Franzke
 * Copyright (c) 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
--------------------------------------------------------------------------*/
/*
    An Open max test lite application ....
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <vidc/media/msm_media_info.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include "OMX_QCOMExtns.h"
#include <sys/time.h>

#include <glib.h>
#define strlcpy g_strlcpy
#define strlcat g_strlcat

//#define ALOGE(fmt, args...) fprintf(stderr, fmt, ##args)
enum {
  PRIO_ERROR=0x1,
  PRIO_INFO=0x1,
  PRIO_HIGH=0x2,
  PRIO_LOW=0x4
};

#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#define getpid() syscall(SYS_getpid)
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define DEBUG_PRINT_CTL(level, fmt, args...)   \
  do {                                        \
    char *ptr = getenv("OMX_DEBUG_LEVEL");    \
    if (level & (ptr?atoi(ptr):0) )           \
       printf("[%ld:%ld]:[%s:%d] " fmt " \n", getpid(), \
       gettid(), __FILENAME__, __LINE__, ##args); \
  } while(0)
#define DEBUG_PRINT(fmt, args...) DEBUG_PRINT_CTL(PRIO_LOW, fmt, ##args)
#define DEBUG_PRINT_ERROR(fmt,args...) DEBUG_PRINT_CTL(PRIO_ERROR, fmt, ##args)
#define ALOGE DEBUG_PRINT_ERROR

#include "OMX_Core.h"
#include "OMX_Component.h"
#include "OMX_QCOMExtns.h"
extern "C" {
#include "queue.h"
}

#include <inttypes.h>

#ifdef USE_ION
#include <linux/msm_ion.h>
#if TARGET_ION_ABI_VERSION >= 2
#include <ion/ion.h>
#include <linux/dma-buf.h>
#else
#include <linux/ion.h>
#endif
#endif
/************************************************************************/
/*              #DEFINES                            */
/************************************************************************/
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
static int previous_vc1_au = 0;
#define CONFIG_VERSION_SIZE(param) \
    param.nVersion.nVersion = CURRENT_OMX_SPEC_VERSION;\
    param.nSize = sizeof(param);

#define FAILED(result) (result != OMX_ErrorNone)

#define SUCCEEDED(result) (result == OMX_ErrorNone)
#define SWAPBYTES(ptrA, ptrB) { char t = *ptrA; *ptrA = *ptrB; *ptrB = t;}
#define SIZE_NAL_FIELD_MAX  4
#define MDP_DEINTERLACE 0x80000000

/************************************************************************/
/*              GLOBAL DECLARATIONS                     */
/************************************************************************/
#ifdef USE_ION
#define PMEM_DEVICE "/dev/ion"
#define MEM_HEAP_ID ION_CP_MM_HEAP_ID

struct vdec_ion {
  int dev_fd;
  int data_fd;
  struct ion_allocation_data alloc_data;
};

bool alloc_map_ion_memory(OMX_U32 buffer_size, struct vdec_ion *ion_info, int flag);
void free_ion_memory(struct vdec_ion *buf_ion_info);
#endif

typedef enum {
  CODEC_FORMAT_H264 = 1,
  CODEC_FORMAT_MP4,
  CODEC_FORMAT_H263,
  CODEC_FORMAT_VC1,
  CODEC_FORMAT_DIVX,
  CODEC_FORMAT_MPEG2,
  CODEC_FORMAT_VP8,
  CODEC_FORMAT_VP9,
  CODEC_FORMAT_HEVC,
  CODEC_FORMAT_MAX
} codec_format;

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

} file_type;

typedef enum {
  GOOD_STATE = 0,
  PORT_SETTING_CHANGE_STATE,
  ERROR_STATE
} test_status;

typedef enum {
  FREE_HANDLE_AT_LOADED = 1,
  FREE_HANDLE_AT_IDLE,
  FREE_HANDLE_AT_EXECUTING,
  FREE_HANDLE_AT_PAUSE
} freeHandle_test;

static int (*Read_Buffer)(OMX_BUFFERHEADERTYPE  *pBufHdr );

int inputBufferFileFd;

FILE * outputBufferFile;
int takeYuvLog = 0;
int displayYuv = 0;
int displayWindow = 0;
int realtime_display = 0;
int num_frames_to_decode = 0;
int thumbnailMode = 0;

Queue *ebd_queue = NULL;
Queue *fbd_queue = NULL;

pthread_t ebd_thread_id;
pthread_t fbd_thread_id;
void* ebd_thread(void*);
void* fbd_thread(void*);

pthread_mutex_t ebd_lock;
pthread_mutex_t fbd_lock;
pthread_mutex_t lock;
pthread_cond_t cond;
pthread_mutex_t eos_lock;
pthread_cond_t eos_cond;
pthread_mutex_t enable_lock;

sem_t ebd_sem;
sem_t fbd_sem;

OMX_PARAM_PORTDEFINITIONTYPE portFmt;
OMX_PORT_PARAM_TYPE portParam;
OMX_ERRORTYPE error;
OMX_COLOR_FORMATTYPE color_fmt;
QOMX_VIDEO_DECODER_PICTURE_ORDER picture_order;

#ifdef MAX_RES_1080P
unsigned int color_fmt_type = 1;
#else
unsigned int color_fmt_type = 0;
#endif
static char tempbuf[16];

int disable_output_port();
int enable_output_port();
int output_port_reconfig();
void free_output_buffers();

/************************************************************************/
/*              GLOBAL INIT                 */
/************************************************************************/
int input_buf_cnt = 0;
int height =0, width =0;
int sliceheight = 0, stride = 0;
int used_ip_buf_cnt = 0;
unsigned free_op_buf_cnt = 0;
volatile int event_is_done = 0;
int ebd_cnt= 0, fbd_cnt = 0;
int bInputEosReached = 0;
int bOutputEosReached = 0;
char in_filename[512];
bool anti_flickering = true;
unsigned etb_count = 0;

OMX_S64 timeStampLfile = 0;
int fps = 30;
unsigned int timestampInterval = 33333;
codec_format  codec_format_option;
file_type     file_type_option;
freeHandle_test freeHandle_option;
int nalSize = 0;
int sent_disabled = 0;
int waitForPortSettingsChanged = 1;
test_status currentStatus = GOOD_STATE;
struct timeval t_start = {0, 0}, t_end = {0, 0};
struct timeval t_main = {0, 0};
int kpi_mode = 0;

//* OMX Spec Version supported by the wrappers. Version = 1.1 */
const OMX_U32 CURRENT_OMX_SPEC_VERSION = 0x00000101;
OMX_COMPONENTTYPE* dec_handle = 0;

OMX_BUFFERHEADERTYPE  **pInputBufHdrs = NULL;
OMX_BUFFERHEADERTYPE  **pOutYUVBufHdrs= NULL;

int rcv_v1=0;

OMX_CONFIG_RECTTYPE crop_rect = {0,0,0,0};

static int bHdrflag = 0;

/************************************************************************/
/*              GLOBAL FUNC DECL                        */
/************************************************************************/
int Init_Decoder();
int Play_Decoder();
int run_tests();

/**************************************************************************/
/*              STATIC DECLARATIONS                       */
/**************************************************************************/
static int video_playback_count = 1;
static int open_video_file ();
static int Read_Buffer_From_DAT_File(OMX_BUFFERHEADERTYPE  *pBufHdr );
static int Read_Buffer_From_H264_Start_Code_File(OMX_BUFFERHEADERTYPE  *pBufHdr);
static int Read_Buffer_From_H265_Start_Code_File(OMX_BUFFERHEADERTYPE  *pBufHdr);
static int Read_Buffer_ArbitraryBytes(OMX_BUFFERHEADERTYPE  *pBufHdr);
static int Read_Buffer_From_Vop_Start_Code_File(OMX_BUFFERHEADERTYPE  *pBufHdr);
static int Read_Buffer_From_Mpeg2_Start_Code(OMX_BUFFERHEADERTYPE  *pBufHdr);
static int Read_Buffer_From_Size_Nal(OMX_BUFFERHEADERTYPE  *pBufHdr);
static int Read_Buffer_From_RCV_File_Seq_Layer(OMX_BUFFERHEADERTYPE  *pBufHdr);
static int Read_Buffer_From_RCV_File(OMX_BUFFERHEADERTYPE  *pBufHdr);
static int Read_Buffer_From_VP8_File(OMX_BUFFERHEADERTYPE  *pBufHdr);
static int Read_Buffer_From_VC1_File(OMX_BUFFERHEADERTYPE  *pBufHdr);
static int Read_Buffer_From_DivX_4_5_6_File(OMX_BUFFERHEADERTYPE  *pBufHdr);
static int Read_Buffer_From_DivX_311_File(OMX_BUFFERHEADERTYPE  *pBufHdr);

static OMX_ERRORTYPE Allocate_Buffer ( OMX_COMPONENTTYPE *dec_handle,
                                       OMX_BUFFERHEADERTYPE  ***pBufHdrs,
                                       OMX_U32 nPortIndex,
                                       long bufCntMin, long bufSize);

static OMX_ERRORTYPE EventHandler(OMX_IN OMX_HANDLETYPE hComponent,
                                  OMX_IN OMX_PTR pAppData,
                                  OMX_IN OMX_EVENTTYPE eEvent,
                                  OMX_IN OMX_U32 nData1, OMX_IN OMX_U32 nData2,
                                  OMX_IN OMX_PTR pEventData);
static OMX_ERRORTYPE EmptyBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                                     OMX_IN OMX_PTR pAppData,
                                     OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);
static OMX_ERRORTYPE FillBufferDone(OMX_OUT OMX_HANDLETYPE hComponent,
                                    OMX_OUT OMX_PTR pAppData,
                                    OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

static void do_freeHandle_and_clean_up(bool isDueToError);

void getFreePmem();

int kpi_place_marker(const char* str)
{
  int fd = open("/sys/kernel/debug/bootkpi/kpi_values", O_WRONLY);
  if(fd >= 0) {
    int ret = write(fd, str, strlen(str));
    close(fd);
    return ret;
  }
  return -1;
}

void wait_for_event(void)
{
  DEBUG_PRINT("Waiting for event");
  pthread_mutex_lock(&lock);
  while (event_is_done == 0) {
    pthread_cond_wait(&cond, &lock);
  }
  event_is_done = 0;
  pthread_mutex_unlock(&lock);
  DEBUG_PRINT("Running .... get the event");
}

void event_complete(void)
{
  pthread_mutex_lock(&lock);
  if (event_is_done == 0) {
    event_is_done = 1;
    pthread_cond_broadcast(&cond);
  }
  pthread_mutex_unlock(&lock);
}

void PrintFramePackArrangement(OMX_QCOM_FRAME_PACK_ARRANGEMENT framePackingArrangement)
{
  DEBUG_PRINT("id (%d)",
     framePackingArrangement.id);
  DEBUG_PRINT("cancel_flag (%d)",
     framePackingArrangement.cancel_flag);
  DEBUG_PRINT("type (%d)",
     framePackingArrangement.type);
  DEBUG_PRINT("quincunx_sampling_flag (%d)",
     framePackingArrangement.quincunx_sampling_flag);
  DEBUG_PRINT("content_interpretation_type (%d)",
     framePackingArrangement.content_interpretation_type);
  DEBUG_PRINT("spatial_flipping_flag (%d)",
     framePackingArrangement.spatial_flipping_flag);
  DEBUG_PRINT("frame0_flipped_flag (%d)",
     framePackingArrangement.frame0_flipped_flag);
  DEBUG_PRINT("field_views_flag (%d)",
     framePackingArrangement.field_views_flag);
  DEBUG_PRINT("current_frame_is_frame0_flag (%d)",
     framePackingArrangement.current_frame_is_frame0_flag);
  DEBUG_PRINT("frame0_self_contained_flag (%d)",
     framePackingArrangement.frame0_self_contained_flag);
  DEBUG_PRINT("frame1_self_contained_flag (%d)",
     framePackingArrangement.frame1_self_contained_flag);
  DEBUG_PRINT("frame0_grid_position_x (%d)",
     framePackingArrangement.frame0_grid_position_x);
  DEBUG_PRINT("frame0_grid_position_y (%d)",
     framePackingArrangement.frame0_grid_position_y);
  DEBUG_PRINT("frame1_grid_position_x (%d)",
     framePackingArrangement.frame1_grid_position_x);
  DEBUG_PRINT("frame1_grid_position_y (%d)",
     framePackingArrangement.frame1_grid_position_y);
  DEBUG_PRINT("reserved_byte (%d)",
     framePackingArrangement.reserved_byte);
  DEBUG_PRINT("repetition_period (%d)",
     framePackingArrangement.repetition_period);
  DEBUG_PRINT("extension_flag (%d)",
     framePackingArrangement.extension_flag);
}

void* ebd_thread(void* pArg)
{
  int signal_eos = 0;
  while(currentStatus != ERROR_STATE)
  {
    int readBytes =0;
    OMX_BUFFERHEADERTYPE* pBuffer = NULL;

    sem_wait(&ebd_sem);
    pthread_mutex_lock(&ebd_lock);
    pBuffer = (OMX_BUFFERHEADERTYPE *) pop(ebd_queue);
    pthread_mutex_unlock(&ebd_lock);
    if(pBuffer == NULL)
    {
      DEBUG_PRINT_ERROR("Error - No ebd pBuffer to dequeue");
      continue;
    }

    if (num_frames_to_decode && (etb_count >= num_frames_to_decode)) {
      DEBUG_PRINT(" Signal EOS %d frames decoded ", num_frames_to_decode);
      signal_eos = 1;
    }

    pBuffer->nOffset = 0;
    if(((readBytes = Read_Buffer(pBuffer)) > 0) && !signal_eos) {
      pBuffer->nFilledLen = readBytes;
      DEBUG_PRINT("%s: Timestamp sent(%lld)", __FUNCTION__, pBuffer->nTimeStamp);
      OMX_EmptyThisBuffer(dec_handle,pBuffer);
      etb_count++;
    }
    else
    {
      pBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
      bInputEosReached = true;
      pBuffer->nFilledLen = readBytes;
      DEBUG_PRINT("%s: Timestamp sent(%lld)", __FUNCTION__, pBuffer->nTimeStamp);
      OMX_EmptyThisBuffer(dec_handle,pBuffer);
      DEBUG_PRINT("EBD::Either EOS or Some Error while reading file");
      etb_count++;
      break;
    }
  }
  return NULL;
}

void* fbd_thread(void* pArg)
{
  long unsigned act_time = 0, display_time = 0, render_time = 5e3, lipsync = 15e3;
  struct timeval t_avsync = {0, 0}, base_avsync = {0, 0};
  float total_time = 0;
  int contigous_drop_frame = 0, bytes_written = 0, ret = 0;
  OMX_S64 base_timestamp = 0, lastTimestamp = 0;
  OMX_BUFFERHEADERTYPE *pBuffer = NULL, *pPrevBuff = NULL;
  pthread_mutex_lock(&eos_lock);
  int stride,scanlines,stride_c,i;
  DEBUG_PRINT("First Inside %s", __FUNCTION__);

  while(currentStatus != ERROR_STATE && !bOutputEosReached)
  {
    pthread_mutex_unlock(&eos_lock);
    DEBUG_PRINT("Inside %s", __FUNCTION__);
    sem_wait(&fbd_sem);
    pthread_mutex_lock(&enable_lock);
    if (sent_disabled)
    {
      pthread_mutex_unlock(&enable_lock);
      pthread_mutex_lock(&fbd_lock);
      if (pPrevBuff != NULL ) {
        if(push(fbd_queue, (void *)pBuffer))
          DEBUG_PRINT_ERROR("Error in enqueueing fbd_data");
        else
          sem_post(&fbd_sem);
        pPrevBuff = NULL;
      }
      if (free_op_buf_cnt == portFmt.nBufferCountActual)
        free_output_buffers();
      pthread_mutex_unlock(&fbd_lock);
      pthread_mutex_lock(&eos_lock);
      continue;
    }
    pthread_mutex_unlock(&enable_lock);
    if (anti_flickering)
      pPrevBuff = pBuffer;
    pthread_mutex_lock(&fbd_lock);
    pBuffer = (OMX_BUFFERHEADERTYPE *)pop(fbd_queue);
    pthread_mutex_unlock(&fbd_lock);
    if (pBuffer == NULL)
    {
      if (anti_flickering)
        pBuffer = pPrevBuff;
      DEBUG_PRINT("Error - No pBuffer to dequeue");
      pthread_mutex_lock(&eos_lock);
      continue;
    }
    else if (pBuffer->nFilledLen > 0)
    {
      if (!fbd_cnt)
      {
        if (kpi_mode == 1) {
          kpi_place_marker("M - Video Decoding 1st frame decoded");
        }
        gettimeofday(&t_start, NULL);
        int time_1st_cost_us = (t_start.tv_sec - t_main.tv_sec) * 1000000 + (t_start.tv_usec - t_main.tv_usec);
        printf("====>The first decoder output frame costs %d.%06d sec.\n",time_1st_cost_us/1000000,time_1st_cost_us%1000000);
      }
      fbd_cnt++;
      DEBUG_PRINT("%s: fbd_cnt(%d) Buf(%p) Timestamp(%lld)",
        __FUNCTION__, fbd_cnt, pBuffer, pBuffer->nTimeStamp);

      if (takeYuvLog)
      {
        if (color_fmt == (OMX_COLOR_FORMATTYPE)QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m)
        {
           DEBUG_PRINT(" width: %d height: %d", crop_rect.nWidth, crop_rect.nHeight);
           unsigned int stride = VENUS_Y_STRIDE(COLOR_FMT_NV12, portFmt.format.video.nFrameWidth);
           unsigned int scanlines = VENUS_Y_SCANLINES(COLOR_FMT_NV12, portFmt.format.video.nFrameHeight);
           char *temp = (char *) pBuffer->pBuffer;
           int i = 0;

           temp += (stride * (int)crop_rect.nTop) +  (int)crop_rect.nLeft;
           for (i = 0; i < crop_rect.nHeight; i++) {
             bytes_written = fwrite(temp, crop_rect.nWidth, 1, outputBufferFile);
             temp += stride;
           }

           temp = (char *)pBuffer->pBuffer + stride * scanlines;
           temp += (stride * (int)crop_rect.nTop) +  (int)crop_rect.nLeft;
           for(i = 0; i < crop_rect.nHeight/2; i++) {
             bytes_written += fwrite(temp, crop_rect.nWidth, 1, outputBufferFile);
             temp += stride;
           }
         }
         else
         {
           bytes_written = fwrite((const char *)pBuffer->pBuffer,
                   pBuffer->nFilledLen,1,outputBufferFile);
         }
         if (bytes_written < 0) {
           DEBUG_PRINT("FillBufferDone: Failed to write to the file");
         }
         else {
           DEBUG_PRINT("FillBufferDone: Wrote %d YUV bytes to the file",
                  bytes_written);
         }
      }
      if (pBuffer->nFlags & OMX_BUFFERFLAG_EXTRADATA)
      {
        OMX_OTHER_EXTRADATATYPE *pExtra;
        DEBUG_PRINT(">> BUFFER WITH EXTRA DATA RCVD <<<");
        pExtra = (OMX_OTHER_EXTRADATATYPE *)
                 ((size_t)(pBuffer->pBuffer + pBuffer->nOffset +
                  pBuffer->nFilledLen + 3)&(~3));
        while(pExtra &&
              (OMX_U8*)pExtra < (pBuffer->pBuffer + pBuffer->nAllocLen) &&
              pExtra->eType != OMX_ExtraDataNone )
        {
          DEBUG_PRINT("ExtraData : pBuf(%p) BufTS(%lld) Type(%x) DataSz(%u)",
               pBuffer, pBuffer->nTimeStamp, pExtra->eType, pExtra->nDataSize);
          switch (pExtra->eType)
          {
            case OMX_ExtraDataInterlaceFormat:
            {
              OMX_STREAMINTERLACEFORMAT *pInterlaceFormat = (OMX_STREAMINTERLACEFORMAT *)pExtra->data;
              DEBUG_PRINT("OMX_ExtraDataInterlaceFormat: Buf(%p) TSmp(%lld) IntPtr(%p) Fmt(%x)",
                pBuffer->pBuffer, pBuffer->nTimeStamp,
                pInterlaceFormat, pInterlaceFormat->nInterlaceFormats);
              break;
            }
            case OMX_ExtraDataFrameInfo:
            {
              OMX_QCOM_EXTRADATA_FRAMEINFO *frame_info = (OMX_QCOM_EXTRADATA_FRAMEINFO *)pExtra->data;
              DEBUG_PRINT("OMX_ExtraDataFrameInfo: Buf(%p) TSmp(%lld) PicType(%u) IntT(%u) ConMB(%u)",
                pBuffer->pBuffer, pBuffer->nTimeStamp, frame_info->ePicType,
                frame_info->interlaceType, frame_info->nConcealedMacroblocks);
              DEBUG_PRINT(" FrmRate(%u), AspRatioX(%u), AspRatioY(%u) DispWidth(%u) DispHeight(%u)",
                frame_info->nFrameRate, frame_info->aspectRatio.aspectRatioX,
                frame_info->aspectRatio.aspectRatioY, frame_info->displayAspectRatio.displayHorizontalSize,
                frame_info->displayAspectRatio.displayVerticalSize);
              DEBUG_PRINT("PANSCAN numWindows(%d)", frame_info->panScan.numWindows);
              for (int i = 0; i < frame_info->panScan.numWindows; i++)
              {
                DEBUG_PRINT("WINDOW Lft(%d) Tp(%d) Rgt(%d) Bttm(%d)",
                  frame_info->panScan.window[i].x,
                  frame_info->panScan.window[i].y,
                  frame_info->panScan.window[i].dx,
                  frame_info->panScan.window[i].dy);
              }
              break;
            }
            break;
            case OMX_ExtraDataConcealMB:
            {
              OMX_U8 data = 0, *data_ptr = (OMX_U8 *)pExtra->data;
              OMX_U32 concealMBnum = 0, bytes_cnt = 0;
              while (bytes_cnt < pExtra->nDataSize)
              {
                data = *data_ptr;
                while (data)
                {
                  concealMBnum += (data&0x01);
                  data >>= 1;
                }
                data_ptr++;
                bytes_cnt++;
              }
              DEBUG_PRINT("OMX_ExtraDataConcealMB: Buf(%p) TSmp(%lld) ConcealMB(%u)",
                pBuffer->pBuffer, pBuffer->nTimeStamp, concealMBnum);
            }
            break;
            case OMX_ExtraDataMP2ExtnData:
            {
              DEBUG_PRINT("OMX_ExtraDataMP2ExtnData");
              OMX_U8 data = 0, *data_ptr = (OMX_U8 *)pExtra->data;
              OMX_U32 bytes_cnt = 0;
              while (bytes_cnt < pExtra->nDataSize)
              {
                DEBUG_PRINT(" MPEG-2 Extension Data Values[%d] = 0x%x", bytes_cnt, *data_ptr);
                data_ptr++;
                bytes_cnt++;
              }
            }
            break;
            case OMX_ExtraDataMP2UserData:
            {
              DEBUG_PRINT("OMX_ExtraDataMP2UserData");
              OMX_U8 data = 0, *data_ptr = (OMX_U8 *)pExtra->data;
              OMX_U32 bytes_cnt = 0;
              while (bytes_cnt < pExtra->nDataSize)
              {
                DEBUG_PRINT(" MPEG-2 User Data Values[%d] = 0x%x", bytes_cnt, *data_ptr);
                data_ptr++;
                bytes_cnt++;
              }
            }
            break;
            default:
              DEBUG_PRINT_ERROR("Unknown Extrata!");
          }
          if (pExtra->nSize < (pBuffer->nAllocLen - (size_t)pExtra))
            pExtra = (OMX_OTHER_EXTRADATATYPE *) (((OMX_U8 *) pExtra) + pExtra->nSize);
          else
          {
            DEBUG_PRINT_ERROR("ERROR: Extradata pointer overflow buffer(%p) extra(%p)",
              pBuffer, pExtra);
            pExtra = NULL;
          }
        }
      }
    }
    if(pBuffer->nFlags & QOMX_VIDEO_BUFFERFLAG_EOSEQ)
    {
      printf("\n");
      printf("***************************************************\n");
      printf("FillBufferDone: End Of Sequence Received\n");
      printf("***************************************************\n");
    }
    if(pBuffer->nFlags & OMX_BUFFERFLAG_DATACORRUPT)
    {
      printf("\n");
      printf("***************************************************\n");
      printf("FillBufferDone: OMX_BUFFERFLAG_DATACORRUPT Received\n");
      printf("***************************************************\n");
    }
    /********************************************************************/
    /* De-Initializing the open max and relasing the buffers and */
    /* closing the files.*/
    /********************************************************************/
    if (pBuffer->nFlags & OMX_BUFFERFLAG_EOS )
    {
      OMX_QCOM_FRAME_PACK_ARRANGEMENT framePackingArrangement;
      OMX_GetConfig(dec_handle,
                   (OMX_INDEXTYPE)OMX_QcomIndexConfigVideoFramePackingArrangement,
                    &framePackingArrangement);
      PrintFramePackArrangement(framePackingArrangement);

      gettimeofday(&t_end, NULL);
      total_time = ((float) ((t_end.tv_sec - t_start.tv_sec) * 1e6
                     + t_end.tv_usec - t_start.tv_usec))/ 1e6;
      //total frames is fbd_cnt - 1 since the start time is
      //recorded after the first frame is decoded.
      DEBUG_PRINT("Avg decoding frame rate=%f", (fbd_cnt - 1)/total_time);

      printf("***************************************************\n");
      printf("FillBufferDone: End Of Stream Reached\n");
      printf("***************************************************\n");
      pthread_mutex_lock(&eos_lock);
      bOutputEosReached = true;
      break;
    }

    pthread_mutex_lock(&enable_lock);
    if (sent_disabled)
    {
      pBuffer->nFilledLen = 0;
      pBuffer->nFlags &= ~OMX_BUFFERFLAG_EXTRADATA;
      pthread_mutex_lock(&fbd_lock);
      if ( pPrevBuff != NULL ) {
        if(push(fbd_queue, (void *)pPrevBuff))
          DEBUG_PRINT_ERROR("Error in enqueueing fbd_data");
        else
          sem_post(&fbd_sem);
        pPrevBuff = NULL;
      }
      if(push(fbd_queue, (void *)pBuffer) < 0)
      {
        DEBUG_PRINT_ERROR("Error in enqueueing fbd_data");
      }
      else
        sem_post(&fbd_sem);
      pthread_mutex_unlock(&fbd_lock);
    }
    else
    {
      if (!anti_flickering)
        pPrevBuff = pBuffer;
      if (pPrevBuff)
      {
        pthread_mutex_lock(&fbd_lock);
        pthread_mutex_lock(&eos_lock);
        if (!bOutputEosReached)
        {
          if ( OMX_FillThisBuffer(dec_handle, pPrevBuff) == OMX_ErrorNone ) {
            free_op_buf_cnt--;
          }
        }
        pthread_mutex_unlock(&eos_lock);
        pthread_mutex_unlock(&fbd_lock);
      }
    }
    pthread_mutex_unlock(&enable_lock);
    pthread_mutex_lock(&eos_lock);
  }

  pthread_cond_broadcast(&eos_cond);
  pthread_mutex_unlock(&eos_lock);
  return NULL;
}

OMX_ERRORTYPE EventHandler(OMX_IN OMX_HANDLETYPE hComponent,
                           OMX_IN OMX_PTR pAppData,
                           OMX_IN OMX_EVENTTYPE eEvent,
                           OMX_IN OMX_U32 nData1, OMX_IN OMX_U32 nData2,
                           OMX_IN OMX_PTR pEventData)
{
  DEBUG_PRINT("Function %s ", __FUNCTION__);

  switch(eEvent) {
    case OMX_EventCmdComplete:
      DEBUG_PRINT(" OMX_EventCmdComplete ");
      if(OMX_CommandPortDisable == (OMX_COMMANDTYPE)nData1)
      {
        printf("*********************************************\n");
        printf("Recieved DISABLE Event Command Complete[%d]\n",nData2);
        printf("*********************************************\n");
      }
      else if(OMX_CommandPortEnable == (OMX_COMMANDTYPE)nData1)
      {
        printf("*********************************************\n");
        printf("Recieved ENABLE Event Command Complete[%d]\n",nData2);
        printf("*********************************************\n");
        if (currentStatus == PORT_SETTING_CHANGE_STATE)
          currentStatus = GOOD_STATE;
        pthread_mutex_lock(&enable_lock);
        sent_disabled = 0;
        pthread_mutex_unlock(&enable_lock);
      }
      else if(OMX_CommandFlush == (OMX_COMMANDTYPE)nData1)
      {
        printf("*********************************************\n");
        printf("Received FLUSH Event Command Complete[%d]\n",nData2);
        printf("*********************************************\n");
      }
      event_complete();
    break;

    case OMX_EventError:
      printf("*********************************************\n");
      printf("Received OMX_EventError Event Command !\n");
      printf("*********************************************\n");
      currentStatus = ERROR_STATE;
      if (OMX_ErrorInvalidState == (OMX_ERRORTYPE)nData1 ||
         OMX_ErrorHardware == (OMX_ERRORTYPE)nData1)
      {
        DEBUG_PRINT("Invalid State or hardware error ");
        if(event_is_done == 0)
        {
          DEBUG_PRINT("Event error in the middle of Decode ");
          pthread_mutex_lock(&eos_lock);
          bOutputEosReached = true;
          pthread_mutex_unlock(&eos_lock);
        }
      }
      if (waitForPortSettingsChanged)
      {
        waitForPortSettingsChanged = 0;
        event_complete();
      }
      sem_post(&ebd_sem);
      sem_post(&fbd_sem);
      break;
    case OMX_EventPortSettingsChanged:
      DEBUG_PRINT("OMX_EventPortSettingsChanged port[%d]", nData1);
      if (nData2 == OMX_IndexConfigCommonOutputCrop) {
        OMX_U32 outPortIndex = 1;
        if (nData1 == outPortIndex) {
          crop_rect.nPortIndex = outPortIndex;
          crop_rect.nSize = sizeof(OMX_CONFIG_RECTTYPE);
          OMX_ERRORTYPE ret = OMX_GetConfig(dec_handle,
                OMX_IndexConfigCommonOutputCrop, &crop_rect);
          if (FAILED(ret)) {
            DEBUG_PRINT_ERROR("Failed to get crop rectangle");
            break;
          } else
            DEBUG_PRINT("Got Crop Rect: (%d, %d) (%d x %d)",
                crop_rect.nLeft, crop_rect.nTop, crop_rect.nWidth, crop_rect.nHeight);
        }
        currentStatus = GOOD_STATE;
        break;
      }

      if (nData2 != OMX_IndexParamPortDefinition)
        break;
      currentStatus = PORT_SETTING_CHANGE_STATE;
      if (waitForPortSettingsChanged)
      {
        waitForPortSettingsChanged = 0;
        event_complete();
      }
      else
      {
        pthread_mutex_lock(&eos_lock);
        pthread_cond_broadcast(&eos_cond);
        pthread_mutex_unlock(&eos_lock);
      }
      break;

    case OMX_EventBufferFlag:
      DEBUG_PRINT("OMX_EventBufferFlag port[%d] flags[%x]", nData1, nData2);
#if 0
            // we should not set the bOutputEosReached here. in stead we wait until fbd_thread to
            // check the flag so that all frames can be dumped for bit exactness check.
            if (nData1 == 1 && (nData2 & OMX_BUFFERFLAG_EOS)) {
                pthread_mutex_lock(&eos_lock);
                bOutputEosReached = true;
                pthread_mutex_unlock(&eos_lock);
            }
            else
            {
                DEBUG_PRINT_ERROR("OMX_EventBufferFlag Event not handled");
            }
#endif
      break;
    case OMX_EventIndexsettingChanged:
      DEBUG_PRINT("OMX_EventIndexSettingChanged Interlace mode[%x]", nData1);
      break;
    default:
      DEBUG_PRINT_ERROR("ERROR - Unknown Event ");
      break;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE EmptyBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                              OMX_IN OMX_PTR pAppData,
                              OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
  int readBytes =0; int bufCnt=0;
  OMX_ERRORTYPE result;

  DEBUG_PRINT("Function %s cnt[%d]", __FUNCTION__, ebd_cnt);
  ebd_cnt++;

  if(bInputEosReached) {
    printf("*****EBD:Input EoS Reached************\n");
    return OMX_ErrorNone;
  }

  pthread_mutex_lock(&ebd_lock);
  if(push(ebd_queue, (void *) pBuffer) < 0)
  {
    DEBUG_PRINT_ERROR("Error in enqueue  ebd data");
    return OMX_ErrorUndefined;
  }
  pthread_mutex_unlock(&ebd_lock);
  sem_post(&ebd_sem);

  return OMX_ErrorNone;
}

OMX_ERRORTYPE FillBufferDone(OMX_OUT OMX_HANDLETYPE hComponent,
                             OMX_OUT OMX_PTR pAppData,
                             OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  DEBUG_PRINT("Inside %s callback_count[%d] ", __FUNCTION__, fbd_cnt);

  /* Test app will assume there is a dynamic port setting
   * In case that there is no dynamic port setting, OMX will not call event cb,
   * instead OMX will send empty this buffer directly and we need to clear an event here
   */
  if(waitForPortSettingsChanged)
  {
    waitForPortSettingsChanged = 0;
    event_complete();
  }

  pthread_mutex_lock(&fbd_lock);
  free_op_buf_cnt++;
  if(push(fbd_queue, (void *)pBuffer) < 0)
  {
    pthread_mutex_unlock(&fbd_lock);
    DEBUG_PRINT_ERROR("Error in enqueueing fbd_data");
    return OMX_ErrorUndefined;
  }
  pthread_mutex_unlock(&fbd_lock);
  sem_post(&fbd_sem);

  return OMX_ErrorNone;
}

int main(int argc, char **argv)
{
  int i=0;
  int bufCnt=0;
  int num=0;
  int outputOption = 0;
  int test_option = 0;
  int pic_order = 0;
  OMX_ERRORTYPE result;
  sliceheight = height = 144;
  stride = width = 176;

  crop_rect.nLeft = 0;
  crop_rect.nTop = 0;
  crop_rect.nWidth = width;
  crop_rect.nHeight = height;


#define KPI_INDICATOR_STR "+kpi+"
  if (argc < 2)
  {
    printf("Usage example(only verified h264 and h265):\n");
    printf("For h264: %s xxx.h264 1 4 2 1 0 0 0\n", argv[0]);
    printf("For h265: %s xxx.h265 9 4 2 1 0 0 0\n", argv[0]);
    printf("Above cmd will output NV12 file as yuvframes.yuv under current directory.\n\n");
    printf("Also could try: %s <input_file>\n", argv[0]);
    printf("It will show prompt, and help you input parameters interactively.\n\n");
    printf("For kpi mode, add %s before input file without blank\n", KPI_INDICATOR_STR);
    printf("kpi example: %s %s/data/xxx.h264 1 4 0 1 0 0 0\n\n", argv[0], KPI_INDICATOR_STR);
    return -1;
  }

  {
    FILE* file = NULL;
    char* infilename_argptr = NULL;
    if (0 == strncmp(argv[1], KPI_INDICATOR_STR, strlen(KPI_INDICATOR_STR))) {
      kpi_mode = 1;
      infilename_argptr = &(argv[1][strlen(KPI_INDICATOR_STR)]);
      if (infilename_argptr[0] == '\0') {
        printf("Missing real input file in cmd line for kpi mode!\n");
        return -1;
      }
      usleep(30000);//For early kpi mode, wait for a while to ensure everything is ready during board bootup.
    }else{
      infilename_argptr = argv[1];
    }
    strlcpy(in_filename, infilename_argptr, sizeof(in_filename));
    file = fopen(in_filename, "rb");
    if (file == NULL) {
      printf("Could not open input file %s !\n", in_filename);
      return -1;
    }else{
      //Here, just confirm input file exist. Later, will open it formally.
      fclose(file);
    }
  }

  if(argc > 2)
  {
    codec_format_option = (codec_format)atoi(argv[2]);
    // file_type, out_op, tst_op, nal_sz, disp_win, rt_dis, (fps), color, pic_order, num_frames_to_decode
    int param[10] = {2, 1, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF};
    int next_arg = 3, idx = 0;
    while (argc > next_arg && idx < 10)
    {
      param[idx++] = atoi(argv[next_arg++]);
    }
    idx = 0;
    file_type_option = (file_type)param[idx++];
    if (codec_format_option == CODEC_FORMAT_H264 && file_type_option == 3)
    {
      nalSize = param[idx++];
      if (nalSize != 2 && nalSize != 4)
      {
        DEBUG_PRINT_ERROR("Error - Can't pass NAL length size = %d", nalSize);
        return -1;
      }
    }
    outputOption = param[idx++];
    test_option = param[idx++];
    if ((outputOption == 1 || outputOption ==3) && test_option != 3) {
      displayWindow = param[idx++];
      if (displayWindow > 0)
        DEBUG_PRINT("Only entire display window supported! Ignoring value");
      realtime_display = param[idx++];
    }
    if (realtime_display)
    {
      takeYuvLog = 0;
      if(param[idx] != 0xFF)
      {
        fps = param[idx++];
        timestampInterval = 1e6 / fps;
      }
    }
    color_fmt_type = (param[idx] != 0xFF)? param[idx++] : color_fmt_type;
    if (test_option != 3) {
      pic_order = (param[idx] != 0xFF)? param[idx++] : 0;
      num_frames_to_decode = param[idx++];
    }
    DEBUG_PRINT("Executing DynPortReconfig QCIF 144 x 176 ");
  }
  else
  {
    printf("Command line argument is available\n");
    printf("To use it: ./mm-vdec-omx-test <clip location> <codec_type> \n");
    printf("           <input_type: 1. per AU(.dat), 2. arbitrary, 3.per NAL/frame>\n");
    printf("           <output_type> <test_case> <size_nal if H264>\n\n\n");
    printf(" *********************************************\n");
    printf(" ENTER THE TEST CASE YOU WOULD LIKE TO EXECUTE\n");
    printf(" *********************************************\n");
    printf(" 1--> H264\n");
    printf(" 2--> MP4\n");
    printf(" 3--> H263\n");
    printf(" 4--> VC1\n");
    printf(" 5--> DivX\n");
    printf(" 6--> MPEG2\n");
    printf(" 7--> VP8\n");
    printf(" 8--> VP9\n");
    printf(" 9--> HEVC\n");
    fflush(stdin);
    if (fgets(tempbuf,sizeof(tempbuf),stdin) <= 0)
      DEBUG_PRINT_ERROR("Error while reading");
    sscanf(tempbuf,"%d",&codec_format_option);
    fflush(stdin);
    if (codec_format_option > CODEC_FORMAT_MAX)
    {
      DEBUG_PRINT_ERROR(" Wrong test case...[%d] ", codec_format_option);
      return -1;
    }
    printf(" *********************************************\n");
    printf(" ENTER THE TEST CASE YOU WOULD LIKE TO EXECUTE\n");
    printf(" *********************************************\n");
    printf(" 1--> PER ACCESS UNIT CLIP (.dat). Clip only available for H264 and Mpeg4\n");
    printf(" 2--> ARBITRARY BYTES (need .264/.264c/.m4v/.263/.rcv/.vc1/.m2v)\n");
    if (codec_format_option == CODEC_FORMAT_H264)
    {
      printf(" 4--> START CODE BASED CLIP (.264/.h264)\n");
    }
    if (codec_format_option == CODEC_FORMAT_HEVC)
    {
      printf(" 4--> START CODE BASED CLIP (.265/.h265)\n");
    }
    else if ( (codec_format_option == CODEC_FORMAT_MP4) || (codec_format_option == CODEC_FORMAT_H263) )
    {
      printf(" 3--> MP4 VOP or H263 P0 SHORT HEADER START CODE CLIP (.m4v or .263)\n");
    }
    else if (codec_format_option == CODEC_FORMAT_VC1)
    {
      printf(" 3--> VC1 clip Simple/Main Profile (.rcv)\n");
      printf(" 4--> VC1 clip Advance Profile (.vc1)\n");
    }
    else if (codec_format_option == CODEC_FORMAT_DIVX)
    {
      printf(" 3--> DivX 4, 5, 6 clip (.cmp)\n");
#ifdef MAX_RES_1080P
      printf(" 4--> DivX 3.11 clip \n");
#endif
    }
    else if (codec_format_option == CODEC_FORMAT_MPEG2)
    {
      printf(" 3--> MPEG2 START CODE CLIP (.m2v)\n");
    }
    else if (codec_format_option == CODEC_FORMAT_VP8)
    {
      printf(" 61--> VP8 START CODE CLIP (.ivf)\n");
    }
    else if (codec_format_option == CODEC_FORMAT_VP9)
    {
      printf(" 61--> VP9 START CODE CLIP (.ivf)\n");
    }
    fflush(stdin);
    if (fgets(tempbuf,sizeof(tempbuf),stdin) <= 0)
      DEBUG_PRINT_ERROR("Error while reading");
    sscanf(tempbuf,"%d",&file_type_option);
    if ( (codec_format_option == CODEC_FORMAT_VP8) || (codec_format_option == CODEC_FORMAT_VP9) )
    {
      file_type_option = FILE_TYPE_VP8;
    }
    fflush(stdin);
    if (codec_format_option == CODEC_FORMAT_H264 && file_type_option == 3)
    {
      printf(" Enter Nal length size [2 or 4] \n");
      if (fgets(tempbuf,sizeof(tempbuf),stdin) <= 0)
        DEBUG_PRINT_ERROR("Error while reading");
      sscanf(tempbuf,"%d",&nalSize);
      if (nalSize != 2 && nalSize != 4)
      {
        DEBUG_PRINT_ERROR("Error - Can't pass NAL length size = %d", nalSize);
        return -1;
      }
    }

    printf(" *********************************************\n");
    printf(" Output buffer option:\n");
    printf(" *********************************************\n");
    printf(" 0 --> No display and no YUV log\n");
    printf(" 1 --> Diplay YUV\n");
    printf(" 2 --> Take YUV log\n");
    printf(" 3 --> Display YUV and take YUV log\n");
    fflush(stdin);
    if (fgets(tempbuf,sizeof(tempbuf),stdin) <= 0)
      DEBUG_PRINT_ERROR("Error while reading");
    sscanf(tempbuf,"%d",&outputOption);
    fflush(stdin);

    printf(" *********************************************\n");
    printf(" ENTER THE TEST CASE YOU WOULD LIKE TO EXECUTE\n");
    printf(" *********************************************\n");
    printf(" 1 --> Play the clip till the end\n");
    printf(" 2 --> Run compliance test. Do NOT expect any display for most option. \n");
    printf("       Please only see \"TEST SUCCESSFULL\" to indicate test pass\n");
    printf(" 3 --> Thumbnail decode mode\n");
    fflush(stdin);
    if (fgets(tempbuf,sizeof(tempbuf),stdin) <= 0)
      DEBUG_PRINT_ERROR("Error while reading");
    sscanf(tempbuf,"%d",&test_option);
    fflush(stdin);
    if (test_option == 3)
      thumbnailMode = 1;

    if ((outputOption == 1 || outputOption == 3) && thumbnailMode == 0)
    {
      printf(" *********************************************\n");
      printf(" ENTER THE PORTION OF DISPLAY TO USE\n");
      printf(" *********************************************\n");
      printf(" 0 --> Entire Screen\n");
      printf(" 1 --> 1/4 th of the screen starting from top left corner to middle \n");
      printf(" 2 --> 1/4 th of the screen starting from middle to top right corner \n");
      printf(" 3 --> 1/4 th of the screen starting from middle to bottom left \n");
      printf(" 4 --> 1/4 th of the screen starting from middle to bottom right \n");
      printf("       Please only see \"TEST SUCCESSFULL\" to indidcate test pass\n");
      fflush(stdin);
      if (fgets(tempbuf,sizeof(tempbuf),stdin) <= 0)
        DEBUG_PRINT_ERROR("Error while reading");
      sscanf(tempbuf,"%d",&displayWindow);
      fflush(stdin);
      if(displayWindow > 0)
      {
        DEBUG_PRINT(" Curently display window 0 only supported; ignoring other values");
        displayWindow = 0;
      }
    }

    if ((outputOption == 1 || outputOption == 3) && thumbnailMode == 0)
    {
      printf(" *********************************************\n");
      printf(" DO YOU WANT TEST APP TO RENDER in Real time \n");
      printf(" 0 --> NO\n 1 --> YES\n");
      printf(" Warning: For H264, it require one NAL per frame clip.\n");
      printf("          For Arbitrary bytes option, Real time display is not recommended\n");
      printf(" *********************************************\n");
      fflush(stdin);
      if (fgets(tempbuf,sizeof(tempbuf),stdin) <= 0)
        DEBUG_PRINT_ERROR("Error while reading");
      sscanf(tempbuf,"%d",&realtime_display);
      fflush(stdin);
    }

    if (realtime_display)
    {
      printf(" *********************************************\n");
      printf(" ENTER THE CLIP FPS\n");
      printf(" Exception: Timestamp extracted from clips will be used.\n");
      printf(" *********************************************\n");
      fflush(stdin);
      if (fgets(tempbuf,sizeof(tempbuf),stdin) <= 0)
        DEBUG_PRINT_ERROR("Error while reading");
      sscanf(tempbuf,"%d",&fps);
      fflush(stdin);
      timestampInterval = 1000000/fps;
    }

    printf(" *********************************************\n");
    printf(" ENTER THE COLOR FORMAT \n");
    printf(" 0 --> Semiplanar \n 1 --> Tile Mode\n");
    printf(" *********************************************\n");
    fflush(stdin);
    if (fgets(tempbuf,sizeof(tempbuf),stdin) <= 0)
      DEBUG_PRINT_ERROR("Error while reading");
    sscanf(tempbuf,"%d",&color_fmt_type);
    fflush(stdin);

    if (thumbnailMode != 1) {
      printf(" *********************************************\n");
      printf(" Output picture order option: \n");
      printf(" *********************************************\n");
      printf(" 0 --> Display order\n 1 --> Decode order\n");
      fflush(stdin);
      if (fgets(tempbuf,sizeof(tempbuf),stdin) <= 0)
        DEBUG_PRINT_ERROR("Error while reading");
      sscanf(tempbuf,"%d",&pic_order);
      fflush(stdin);

      printf(" *********************************************\n");
      printf(" Number of frames to decode: \n");
      printf(" 0 ---> decode all frames: \n");
      printf(" *********************************************\n");
      fflush(stdin);
      if (fgets(tempbuf,sizeof(tempbuf),stdin) <= 0)
        DEBUG_PRINT_ERROR("Error while reading");
      sscanf(tempbuf,"%d",&num_frames_to_decode);
      fflush(stdin);
    }
  }
  if (file_type_option >= FILE_TYPE_COMMON_CODEC_MAX)
  {
    switch (codec_format_option)
    {
      case CODEC_FORMAT_H264:
        file_type_option = (file_type)(FILE_TYPE_START_OF_H264_SPECIFIC + file_type_option - FILE_TYPE_COMMON_CODEC_MAX);
        break;
      case CODEC_FORMAT_DIVX:
        file_type_option = (file_type)(FILE_TYPE_START_OF_DIVX_SPECIFIC + file_type_option - FILE_TYPE_COMMON_CODEC_MAX);
        break;
      case CODEC_FORMAT_MP4:
      case CODEC_FORMAT_H263:
        file_type_option = (file_type)(FILE_TYPE_START_OF_MP4_SPECIFIC + file_type_option - FILE_TYPE_COMMON_CODEC_MAX);
        break;
      case CODEC_FORMAT_VC1:
        file_type_option = (file_type)(FILE_TYPE_START_OF_VC1_SPECIFIC + file_type_option - FILE_TYPE_COMMON_CODEC_MAX);
        break;
      case CODEC_FORMAT_MPEG2:
        file_type_option = (file_type)(FILE_TYPE_START_OF_MPEG2_SPECIFIC + file_type_option - FILE_TYPE_COMMON_CODEC_MAX);
        break;
      case CODEC_FORMAT_VP8:
        break;
      case CODEC_FORMAT_HEVC:
        file_type_option = (file_type)(FILE_TYPE_START_OF_H265_SPECIFIC + file_type_option - FILE_TYPE_COMMON_CODEC_MAX);
        break;
      default:
        DEBUG_PRINT_ERROR("Error: Unknown code %d", codec_format_option);
    }
  }

  CONFIG_VERSION_SIZE(picture_order);
  picture_order.eOutputPictureOrder = QOMX_VIDEO_DISPLAY_ORDER;
  if (pic_order == 1)
    picture_order.eOutputPictureOrder = QOMX_VIDEO_DECODE_ORDER;

  if (outputOption == 0)
  {
    displayYuv = 0;
    takeYuvLog = 0;
    realtime_display = 0;
  }
  else if (outputOption == 1)
  {
    displayYuv = 1;
  }
  else if (outputOption == 2)
  {
    takeYuvLog = 1;
    realtime_display = 0;
  }
  else if (outputOption == 3)
  {
    displayYuv = 1;
    takeYuvLog = !realtime_display;
  }
  else
  {
    DEBUG_PRINT("Wrong option. Assume you want to see the YUV display");
    displayYuv = 1;
  }

  if (test_option == 2)
  {
    printf(" *********************************************\n");
    printf(" ENTER THE COMPLIANCE TEST YOU WOULD LIKE TO EXECUTE\n");
    printf(" *********************************************\n");
    printf(" 1 --> Call Free Handle at the OMX_StateLoaded\n");
    printf(" 2 --> Call Free Handle at the OMX_StateIdle\n");
    printf(" 3 --> Call Free Handle at the OMX_StateExecuting\n");
    printf(" 4 --> Call Free Handle at the OMX_StatePause\n");
    fflush(stdin);
    if (fgets(tempbuf,sizeof(tempbuf),stdin) <= 0)
      DEBUG_PRINT_ERROR("Error while reading");
    sscanf(tempbuf,"%d",&freeHandle_option);
    fflush(stdin);
  }
  else
  {
    freeHandle_option = (freeHandle_test)0;
  }

  printf("Input values: inputfilename[%s]\n", in_filename);
  printf("*******************************************************\n");
  gettimeofday(&t_main, NULL);
  if (kpi_mode == 1) {
    kpi_place_marker("M - Video Decoding begin preparing");
  }
  pthread_cond_init(&cond, 0);
  pthread_cond_init(&eos_cond, 0);
  pthread_mutex_init(&eos_lock, 0);
  pthread_mutex_init(&lock, 0);
  pthread_mutex_init(&ebd_lock, 0);
  pthread_mutex_init(&fbd_lock, 0);
  pthread_mutex_init(&enable_lock, 0);
  if (-1 == sem_init(&ebd_sem, 0, 0))
  {
    DEBUG_PRINT_ERROR("Error - sem_init failed %d", errno);
  }
  if (-1 == sem_init(&fbd_sem, 0, 0))
  {
    DEBUG_PRINT_ERROR("Error - sem_init failed %d", errno);
  }
  ebd_queue = alloc_queue();
  if (ebd_queue == NULL)
  {
    DEBUG_PRINT_ERROR(" Error in Creating ebd_queue");
    return -1;
  }

  fbd_queue = alloc_queue();
  if (fbd_queue == NULL)
  {
    DEBUG_PRINT_ERROR(" Error in Creating fbd_queue");
    free_queue(ebd_queue);
    return -1;
  }

  if(0 != pthread_create(&fbd_thread_id, NULL, fbd_thread, NULL))
  {
    DEBUG_PRINT_ERROR(" Error in Creating fbd_thread ");
    free_queue(ebd_queue);
    free_queue(fbd_queue);
    return -1;
  }

  run_tests();

  pthread_cond_destroy(&cond);
  pthread_mutex_destroy(&lock);
  pthread_mutex_destroy(&ebd_lock);
  pthread_mutex_destroy(&fbd_lock);
  pthread_mutex_destroy(&enable_lock);
  pthread_cond_destroy(&eos_cond);
  pthread_mutex_destroy(&eos_lock);
  if (-1 == sem_destroy(&ebd_sem))
  {
    DEBUG_PRINT_ERROR("Error - sem_destroy failed %d", errno);
  }
  if (-1 == sem_destroy(&fbd_sem))
  {
    DEBUG_PRINT_ERROR("Error - sem_destroy failed %d", errno);
  }
  return 0;
}

int run_tests()
{
  int cmd_error = 0;
  DEBUG_PRINT("Inside %s", __FUNCTION__);
  waitForPortSettingsChanged = 1;

  if(file_type_option == FILE_TYPE_DAT_PER_AU) {
    Read_Buffer = Read_Buffer_From_DAT_File;
  }
  else if(file_type_option == FILE_TYPE_ARBITRARY_BYTES) {
    Read_Buffer = Read_Buffer_ArbitraryBytes;
  }
  else if(codec_format_option == CODEC_FORMAT_H264) {
    if (file_type_option == FILE_TYPE_264_NAL_SIZE_LENGTH) {
      Read_Buffer = Read_Buffer_From_Size_Nal;
    } else if (file_type_option == FILE_TYPE_264_START_CODE_BASED) {
      Read_Buffer = Read_Buffer_From_H264_Start_Code_File;
    } else {
      DEBUG_PRINT_ERROR("Invalid file_type_option(%d) for H264", file_type_option);
      return -1;
    }
  }
  else if(codec_format_option == CODEC_FORMAT_HEVC) {
    if (file_type_option == FILE_TYPE_265_START_CODE_BASED) {
      Read_Buffer = Read_Buffer_From_H265_Start_Code_File;
    } else {
      DEBUG_PRINT_ERROR("Invalid file_type_option(%d) for HEVC", file_type_option);
      return -1;
    }
  }
  else if((codec_format_option == CODEC_FORMAT_H263) ||
          (codec_format_option == CODEC_FORMAT_MP4)) {
    Read_Buffer = Read_Buffer_From_Vop_Start_Code_File;
  }
  else if (codec_format_option == CODEC_FORMAT_MPEG2) {
    Read_Buffer = Read_Buffer_From_Mpeg2_Start_Code;
  }
  else if(file_type_option == FILE_TYPE_DIVX_4_5_6) {
    Read_Buffer = Read_Buffer_From_DivX_4_5_6_File;
  }
#ifdef MAX_RES_1080P
  else if(file_type_option == FILE_TYPE_DIVX_311) {
    Read_Buffer = Read_Buffer_From_DivX_311_File;
  }
#endif
  else if(file_type_option == FILE_TYPE_RCV) {
    Read_Buffer = Read_Buffer_From_RCV_File;
  }
  else if(file_type_option == FILE_TYPE_VP8) {
    Read_Buffer = Read_Buffer_From_VP8_File;
  }
  else if(file_type_option == FILE_TYPE_VC1) {
    Read_Buffer = Read_Buffer_From_VC1_File;
  }

  DEBUG_PRINT("file_type_option %d!", file_type_option);

  switch(file_type_option)
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
    case FILE_TYPE_265_START_CODE_BASED:
    case FILE_TYPE_DIVX_4_5_6:
#ifdef MAX_RES_1080P
    case FILE_TYPE_DIVX_311:
#endif
      if(Init_Decoder()!= 0x00)
      {
        DEBUG_PRINT_ERROR("Error - Decoder Init failed");
        return -1;
      }
      if(Play_Decoder() != 0x00)
      {
        return -1;
      }
      break;
    default:
      DEBUG_PRINT_ERROR("Error - Invalid Entry...%d",file_type_option);
      break;
  }

  anti_flickering = true;

  pthread_mutex_lock(&eos_lock);
  while (bOutputEosReached == false && cmd_error == 0)
  {
    pthread_cond_wait(&eos_cond, &eos_lock);

    if (currentStatus == PORT_SETTING_CHANGE_STATE)
    {
      pthread_mutex_unlock(&eos_lock);
      cmd_error = output_port_reconfig();
      pthread_mutex_lock(&eos_lock);
    }
  }
  pthread_mutex_unlock(&eos_lock);

  // Wait till EOS is reached...
  if(bOutputEosReached)
    do_freeHandle_and_clean_up(currentStatus == ERROR_STATE);
  return 0;
}

int Init_Decoder()
{
  DEBUG_PRINT("Inside %s ", __FUNCTION__);
  OMX_ERRORTYPE omxresult;
  OMX_U32 total = 0;
  char vdecCompNames[50];
  typedef OMX_U8* OMX_U8_PTR;
  char *role = (char *) "video_decoder";

  static OMX_CALLBACKTYPE call_back = {&EventHandler, &EmptyBufferDone, &FillBufferDone};

  int i = 0;
  long bufCnt = 0;

  /* Init. the OpenMAX Core */
  DEBUG_PRINT("Initializing OpenMAX Core....");
  omxresult = OMX_Init();

  if(OMX_ErrorNone != omxresult) {
    DEBUG_PRINT_ERROR(" Failed to Init OpenMAX core");
    return -1;
  }
  else {
    DEBUG_PRINT_ERROR("OpenMAX Core Init Done");
  }

  if (codec_format_option == CODEC_FORMAT_H264)
  {
    strlcpy(vdecCompNames, "OMX.qcom.video.decoder.avc", sizeof(vdecCompNames));
  }
  else if (codec_format_option == CODEC_FORMAT_MP4)
  {
    strlcpy(vdecCompNames, "OMX.qcom.video.decoder.mpeg4", sizeof(vdecCompNames));
  }
  else if (codec_format_option == CODEC_FORMAT_H263)
  {
    strlcpy(vdecCompNames, "OMX.qcom.video.decoder.h263", sizeof(vdecCompNames));
  }
  else if (codec_format_option == CODEC_FORMAT_VC1)
  {
    strlcpy(vdecCompNames, "OMX.qcom.video.decoder.vc1", sizeof(vdecCompNames));
  }
  else if (codec_format_option == CODEC_FORMAT_MPEG2)
  {
    strlcpy(vdecCompNames, "OMX.qcom.video.decoder.mpeg2", sizeof(vdecCompNames));
  }
  else if (file_type_option == FILE_TYPE_RCV)
  {
    strlcpy(vdecCompNames, "OMX.qcom.video.decoder.wmv", sizeof(vdecCompNames));
  }
  else if (file_type_option == FILE_TYPE_DIVX_4_5_6)
  {
    strlcpy(vdecCompNames, "OMX.qcom.video.decoder.divx", sizeof(vdecCompNames));
  }
  else if (codec_format_option == CODEC_FORMAT_VP8)
  {
    strlcpy(vdecCompNames, "OMX.qcom.video.decoder.vp8", sizeof(vdecCompNames));
  }
  else if (codec_format_option == CODEC_FORMAT_VP9)
  {
    strlcpy(vdecCompNames, "OMX.qcom.video.decoder.vp9", sizeof(vdecCompNames));
  }
  else if (codec_format_option == CODEC_FORMAT_HEVC)
  {
    strlcpy(vdecCompNames, "OMX.qcom.video.decoder.hevc", sizeof(vdecCompNames));
  }
#ifdef MAX_RES_1080P
  else if (file_type_option == FILE_TYPE_DIVX_311)
  {
    strlcpy(vdecCompNames, "OMX.qcom.video.decoder.divx311", sizeof(vdecCompNames));
  }
#endif
  else
  {
    DEBUG_PRINT_ERROR("Error: Unsupported codec %d", codec_format_option);
    return -1;
  }

  omxresult = OMX_GetHandle((OMX_HANDLETYPE*)(&dec_handle),
                    (OMX_STRING)vdecCompNames, NULL, &call_back);
  if (FAILED(omxresult)) {
    DEBUG_PRINT_ERROR("Failed to Load the component:%s", vdecCompNames);
    printf("Failed to Load the component:%s, OMX_GetHandle() ret 0x%08x\n", vdecCompNames, omxresult);
    if (kpi_mode == 1) {
      char msg[128] = {0};
      snprintf(msg, sizeof(msg), "E - Video Dec getHandle 0x%08x err", omxresult);
      kpi_place_marker(msg);
    }
    return -1;
  }
  else
  {
    DEBUG_PRINT("Component %s is in LOADED state", vdecCompNames);
  }

  QOMX_VIDEO_QUERY_DECODER_INSTANCES decoder_instances;
  omxresult = OMX_GetConfig(dec_handle,
           (OMX_INDEXTYPE)OMX_QcomIndexQueryNumberOfVideoDecInstance,
           &decoder_instances);
  DEBUG_PRINT(" Number of decoder instances %d",
           decoder_instances.nNumOfInstances);

  /* Get the port information */
  CONFIG_VERSION_SIZE(portParam);
  omxresult = OMX_GetParameter(dec_handle, OMX_IndexParamVideoInit,
           (OMX_PTR)&portParam);

  if(FAILED(omxresult)) {
    DEBUG_PRINT_ERROR("ERROR - Failed to get Port Param");
    return -1;
  }
  else
  {
    DEBUG_PRINT("portParam.nPorts:%d", portParam.nPorts);
    DEBUG_PRINT("portParam.nStartPortNumber:%d", portParam.nStartPortNumber);
  }

  /* Set the compression format on i/p port */
  if (codec_format_option == CODEC_FORMAT_H264)
  {
    portFmt.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  }
  else if (codec_format_option == CODEC_FORMAT_MP4)
  {
    portFmt.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  }
  else if (codec_format_option == CODEC_FORMAT_H263)
  {
    portFmt.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
  }
  else if (codec_format_option == CODEC_FORMAT_VC1)
  {
    portFmt.format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;
  }
  else if (codec_format_option == CODEC_FORMAT_DIVX)
  {
    portFmt.format.video.eCompressionFormat =
        (OMX_VIDEO_CODINGTYPE)QOMX_VIDEO_CodingDivx;
  }
  else if (codec_format_option == CODEC_FORMAT_MPEG2)
  {
    portFmt.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
  }
  else if (codec_format_option == CODEC_FORMAT_HEVC)
  {
    portFmt.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)QOMX_VIDEO_CodingHevc;
  }
  else
  {
    DEBUG_PRINT_ERROR("Error: Unsupported codec %d", codec_format_option);
  }

  if (thumbnailMode == 1) {
    QOMX_ENABLETYPE thumbNailMode;
    thumbNailMode.bEnable = OMX_TRUE;
    OMX_SetParameter(dec_handle,(OMX_INDEXTYPE)OMX_QcomIndexParamVideoSyncFrameDecodingMode,
              (OMX_PTR)&thumbNailMode);
    DEBUG_PRINT("Enabled Thumbnail mode");
  }

  return 0;
}

int Play_Decoder()
{
  OMX_VIDEO_PARAM_PORTFORMATTYPE videoportFmt = {0};
  int i, bufCnt, index = 0;
  int frameSize=0;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE* pBuffer = NULL;
  DEBUG_PRINT("Inside %s ", __FUNCTION__);

  /* open the i/p and o/p files based on the video file format passed */
  if(open_video_file()) {
    DEBUG_PRINT_ERROR("Error in opening video file");
    return -1;
  }

  OMX_QCOM_PARAM_PORTDEFINITIONTYPE inputPortFmt;
  memset(&inputPortFmt, 0, sizeof(OMX_QCOM_PARAM_PORTDEFINITIONTYPE));
  CONFIG_VERSION_SIZE(inputPortFmt);
  inputPortFmt.nPortIndex = 0;  // input port
  switch (file_type_option)
  {
    case FILE_TYPE_DAT_PER_AU:
    case FILE_TYPE_PICTURE_START_CODE:
    case FILE_TYPE_MPEG2_START_CODE:
    case FILE_TYPE_264_START_CODE_BASED:
    case FILE_TYPE_265_START_CODE_BASED:
    case FILE_TYPE_RCV:
    case FILE_TYPE_VC1:
#ifdef MAX_RES_1080P
    case FILE_TYPE_DIVX_311:
#endif
    {
      inputPortFmt.nFramePackingFormat = OMX_QCOM_FramePacking_OnlyOneCompleteFrame;
      break;
    }

    case FILE_TYPE_ARBITRARY_BYTES:
    case FILE_TYPE_264_NAL_SIZE_LENGTH:
    case FILE_TYPE_DIVX_4_5_6:
    {
      inputPortFmt.nFramePackingFormat = OMX_QCOM_FramePacking_Arbitrary;
      break;
    }
    case FILE_TYPE_VP8:
    {
      inputPortFmt.nFramePackingFormat = OMX_QCOM_FramePacking_OnlyOneCompleteFrame;
      break;
    }
    default:
      inputPortFmt.nFramePackingFormat = OMX_QCOM_FramePacking_Unspecified;
  }
  OMX_SetParameter(dec_handle,(OMX_INDEXTYPE)OMX_QcomIndexPortDefn,
             (OMX_PTR)&inputPortFmt);

  QOMX_ENABLETYPE extra_data;
  extra_data.bEnable = OMX_TRUE;

  OMX_SetParameter(dec_handle,(OMX_INDEXTYPE)OMX_QcomIndexParamFrameInfoExtraData,
           (OMX_PTR)&extra_data);

  /* Query the decoder outport's min buf requirements */
  CONFIG_VERSION_SIZE(portFmt);

  /* Port for which the Client needs to obtain info */
  portFmt.nPortIndex = portParam.nStartPortNumber;

  OMX_GetParameter(dec_handle,OMX_IndexParamPortDefinition,&portFmt);
  DEBUG_PRINT("Dec: Min Buffer Count %d", portFmt.nBufferCountMin);
  DEBUG_PRINT("Dec: Buffer Size %d", portFmt.nBufferSize);

  if(OMX_DirInput != portFmt.eDir) {
    DEBUG_PRINT ("Dec: Expect Input Port");
    return -1;
  }
#ifdef MAX_RES_1080P
  if( (codec_format_option == CODEC_FORMAT_DIVX) &&
    (file_type_option == FILE_TYPE_DIVX_311) ) {

    int off;

    if ( read(inputBufferFileFd, &width, 4 ) == -1 ) {
      DEBUG_PRINT_ERROR("Failed to read width for divx");
      return  -1;
    }

    DEBUG_PRINT("Width for DIVX = %d", width);

    if ( read(inputBufferFileFd, &height, 4 ) == -1 ) {
      DEBUG_PRINT_ERROR("Failed to read height for divx");
      return  -1;
    }

    DEBUG_PRINT("Height for DIVX = %u", height);
    sliceheight = height;
    stride = width;
  }
#endif
  if( (codec_format_option == CODEC_FORMAT_VC1) &&
    (file_type_option == FILE_TYPE_RCV) ) {
    //parse struct_A data to get height and width information
    unsigned int temp;
    lseek64(inputBufferFileFd, 0, SEEK_SET);
    if (read(inputBufferFileFd, &temp, 4) < 0) {
      DEBUG_PRINT_ERROR("Failed to read vc1 data");
      return -1;
    }
    //Refer to Annex L of SMPTE 421M-2006 VC1 decoding standard
    //We need to skip 12 bytes after 0xC5 in sequence layer data
    //structure to read struct_A, which includes height and
    //width information.
    if ((temp & 0xFF000000) == 0xC5000000) {
      lseek64(inputBufferFileFd, 12, SEEK_SET);

      if ( read(inputBufferFileFd, &height, 4 ) < -1 ) {
        DEBUG_PRINT_ERROR("Failed to read height for vc-1");
        return  -1;
      }
      if ( read(inputBufferFileFd, &width, 4 ) == -1 ) {
        DEBUG_PRINT_ERROR("Failed to read width for vc-1");
        return  -1;
      }
      lseek64(inputBufferFileFd, 0, SEEK_SET);
    }
    if ((temp & 0xFF000000) == 0x85000000) {
      lseek64(inputBufferFileFd, 0, SEEK_SET);
    }
    DEBUG_PRINT(" RCV clip width = %u height = %u ",width, height);
  }
  crop_rect.nWidth = width;
  crop_rect.nHeight = height;

  bufCnt = 0;
  portFmt.format.video.nFrameHeight = height;
  portFmt.format.video.nFrameWidth  = width;
  portFmt.format.video.xFramerate = fps;
  OMX_SetParameter(dec_handle,OMX_IndexParamPortDefinition, (OMX_PTR)&portFmt);
  OMX_GetParameter(dec_handle,OMX_IndexParamPortDefinition, &portFmt);
  DEBUG_PRINT("Dec: New Min Buffer Count %d", portFmt.nBufferCountMin);
  CONFIG_VERSION_SIZE(videoportFmt);

  color_fmt = (OMX_COLOR_FORMATTYPE)
       QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m;

  while (ret == OMX_ErrorNone)
  {
    videoportFmt.nPortIndex = 1;
    videoportFmt.nIndex = index;
    ret = OMX_GetParameter(dec_handle, OMX_IndexParamVideoPortFormat,
      (OMX_PTR)&videoportFmt);

    if((ret == OMX_ErrorNone) && (videoportFmt.eColorFormat ==
         color_fmt))
    {
      DEBUG_PRINT(" Format[%u] supported by OMX Decoder", color_fmt);
      break;
    }
    index++;
  }

  if(ret == OMX_ErrorNone)
  {
    if(OMX_SetParameter(dec_handle, OMX_IndexParamVideoPortFormat,
        (OMX_PTR)&videoportFmt) != OMX_ErrorNone)
    {
      DEBUG_PRINT_ERROR(" Setting Tile format failed");
      return -1;
    }
  }
  else
  {
    DEBUG_PRINT_ERROR(" Error in retrieving supported color formats");
    return -1;
  }
  picture_order.nPortIndex = 1;
  DEBUG_PRINT("Set picture order");
  if(OMX_SetParameter(dec_handle,
   (OMX_INDEXTYPE)OMX_QcomIndexParamVideoDecoderPictureOrder,
     (OMX_PTR)&picture_order) != OMX_ErrorNone)
  {
    DEBUG_PRINT_ERROR(" ERROR: Setting picture order!");
    return -1;
  }
  DEBUG_PRINT("Video format: W x H (%d x %d)",
    portFmt.format.video.nFrameWidth,
    portFmt.format.video.nFrameHeight);
  if(codec_format_option == CODEC_FORMAT_H264 ||
     codec_format_option == CODEC_FORMAT_HEVC)
  {
    OMX_VIDEO_CONFIG_NALSIZE naluSize;
    naluSize.nSize = sizeof(OMX_VIDEO_CONFIG_NALSIZE);
    naluSize.nNaluBytes = nalSize;
    DEBUG_PRINT(" Nal length is %d index %d",nalSize,OMX_IndexConfigVideoNalSize);
    OMX_SetConfig(dec_handle,OMX_IndexConfigVideoNalSize,(OMX_PTR)&naluSize);
    DEBUG_PRINT("SETTING THE NAL SIZE to %d",naluSize.nNaluBytes);
  }
  DEBUG_PRINT("OMX_SendCommand Decoder -> IDLE");
  OMX_SendCommand(dec_handle, OMX_CommandStateSet, OMX_StateIdle,0);

  input_buf_cnt = portFmt.nBufferCountActual;
  DEBUG_PRINT("Transition to Idle State succesful...");

  // Allocate buffer on decoder's i/p port
  error = Allocate_Buffer(dec_handle, &pInputBufHdrs, portFmt.nPortIndex,
                portFmt.nBufferCountActual, portFmt.nBufferSize);
  if (error != OMX_ErrorNone) {
    DEBUG_PRINT_ERROR("Error - OMX_AllocateBuffer Input buffer error");
     return -1;
  }
  else {
    DEBUG_PRINT("OMX_AllocateBuffer Input buffer success");
  }

  portFmt.nPortIndex = portParam.nStartPortNumber+1;
  // Port for which the Client needs to obtain info

  OMX_GetParameter(dec_handle,OMX_IndexParamPortDefinition,&portFmt);
  DEBUG_PRINT("nMin Buffer Count=%d", portFmt.nBufferCountMin);
  DEBUG_PRINT("nBuffer Size=%d", portFmt.nBufferSize);
  if(OMX_DirOutput != portFmt.eDir) {
    DEBUG_PRINT_ERROR("Error - Expect Output Port");
    return -1;
  }

  if (anti_flickering) {
    ret = OMX_GetParameter(dec_handle,OMX_IndexParamPortDefinition,&portFmt);
    if (ret != OMX_ErrorNone) {
      DEBUG_PRINT_ERROR("%s: OMX_GetParameter failed: %d",__FUNCTION__, ret);
      return -1;
    }
    portFmt.nBufferCountActual += 1;
    ret = OMX_SetParameter(dec_handle,OMX_IndexParamPortDefinition,&portFmt);
    if (ret != OMX_ErrorNone) {
      DEBUG_PRINT_ERROR("%s: OMX_SetParameter failed: %d",__FUNCTION__, ret);
      return -1;
    }
  }

  /* Allocate buffer on decoder's o/p port */
  error = Allocate_Buffer(dec_handle, &pOutYUVBufHdrs, portFmt.nPortIndex,
                  portFmt.nBufferCountActual, portFmt.nBufferSize);
  free_op_buf_cnt = portFmt.nBufferCountActual;
  if (error != OMX_ErrorNone) {
    DEBUG_PRINT_ERROR("Error - OMX_AllocateBuffer Output buffer error");
    return -1;
  }
  else
  {
    DEBUG_PRINT("OMX_AllocateBuffer Output buffer success");
  }

  wait_for_event();
  if (currentStatus == ERROR_STATE)
  {
    do_freeHandle_and_clean_up(true);
    return -1;
  }

  if (freeHandle_option == FREE_HANDLE_AT_IDLE)
  {
    OMX_STATETYPE state = OMX_StateInvalid;
    OMX_GetState(dec_handle, &state);
    if (state == OMX_StateIdle)
    {
      DEBUG_PRINT("Decoder is in OMX_StateIdle and trying to call OMX_FreeHandle ");
      do_freeHandle_and_clean_up(false);
    }
    else
    {
      DEBUG_PRINT_ERROR("Error - Decoder is in state %d and trying to call OMX_FreeHandle ", state);
      do_freeHandle_and_clean_up(true);
    }
    return -1;
  }

  DEBUG_PRINT("OMX_SendCommand Decoder -> Executing");
  OMX_SendCommand(dec_handle, OMX_CommandStateSet, OMX_StateExecuting,0);
  wait_for_event();
  if (currentStatus == ERROR_STATE)
  {
    do_freeHandle_and_clean_up(true);
    return -1;
  }
  if (pOutYUVBufHdrs == NULL)
  {
    DEBUG_PRINT_ERROR("Error - pOutYUVBufHdrs is NULL");
    return -1;
  }
  for(bufCnt=0; bufCnt < portFmt.nBufferCountActual; ++bufCnt) {
    DEBUG_PRINT("OMX_FillThisBuffer on output buf no.%d",bufCnt);
    if (pOutYUVBufHdrs[bufCnt] == NULL)
    {
      DEBUG_PRINT_ERROR("Error - pOutYUVBufHdrs[%d] is NULL", bufCnt);
      return -1;
    }
    pOutYUVBufHdrs[bufCnt]->nOutputPortIndex = 1;
    pOutYUVBufHdrs[bufCnt]->nFlags &= ~OMX_BUFFERFLAG_EOS;
    ret = OMX_FillThisBuffer(dec_handle, pOutYUVBufHdrs[bufCnt]);
    if (OMX_ErrorNone != ret)
      DEBUG_PRINT_ERROR("Error - OMX_FillThisBuffer failed with result %d", ret);
    else
    {
      DEBUG_PRINT("OMX_FillThisBuffer success!");
      free_op_buf_cnt--;
    }
  }

  used_ip_buf_cnt = input_buf_cnt;

  rcv_v1 = 0;

  if (codec_format_option == CODEC_FORMAT_VC1)
  {
    pInputBufHdrs[0]->nOffset = 0;
    if(file_type_option == FILE_TYPE_RCV)
    {
      frameSize = Read_Buffer_From_RCV_File_Seq_Layer(pInputBufHdrs[0]);
      pInputBufHdrs[0]->nFilledLen = frameSize;
      DEBUG_PRINT("After Read_Buffer_From_RCV_File_Seq_Layer, "
              "frameSize %d", frameSize);
    }
    else if(file_type_option == FILE_TYPE_VC1)
    {
      bHdrflag = 1;
      pInputBufHdrs[0]->nFilledLen = Read_Buffer(pInputBufHdrs[0]);
      bHdrflag = 0;
      DEBUG_PRINT_ERROR("After 1st Read_Buffer for VC1, "
             "pInputBufHdrs[0]->nFilledLen %d", pInputBufHdrs[0]->nFilledLen);
    }
    else
    {
      pInputBufHdrs[0]->nFilledLen = Read_Buffer(pInputBufHdrs[0]);
      DEBUG_PRINT("After Read_Buffer pInputBufHdrs[0]->nFilledLen %d",
             pInputBufHdrs[0]->nFilledLen);
    }

    pInputBufHdrs[0]->nInputPortIndex = 0;
    pInputBufHdrs[0]->nOffset = 0;
    ret = OMX_EmptyThisBuffer(dec_handle, pInputBufHdrs[0]);
    if (ret != OMX_ErrorNone)
    {
      DEBUG_PRINT_ERROR("ERROR - OMX_EmptyThisBuffer failed with result %d", ret);
      do_freeHandle_and_clean_up(true);
      return -1;
    }
    else
    {
      etb_count++;
      DEBUG_PRINT("OMX_EmptyThisBuffer success!");
    }
    i = 1;
    pInputBufHdrs[0]->nFlags = 0;
  }
  else
  {
    i = 0;
  }

  for (i; i < used_ip_buf_cnt;i++) {
    pInputBufHdrs[i]->nInputPortIndex = 0;
    pInputBufHdrs[i]->nOffset = 0;
    if((frameSize = Read_Buffer(pInputBufHdrs[i])) <= 0 ){
      DEBUG_PRINT("NO FRAME READ");
      pInputBufHdrs[i]->nFilledLen = frameSize;
      pInputBufHdrs[i]->nInputPortIndex = 0;
      pInputBufHdrs[i]->nFlags |= OMX_BUFFERFLAG_EOS;;
      bInputEosReached = true;
      OMX_EmptyThisBuffer(dec_handle, pInputBufHdrs[i]);
      etb_count++;
      DEBUG_PRINT("File is small::Either EOS or Some Error while reading file");
      break;
    }
    pInputBufHdrs[i]->nFilledLen = frameSize;
    pInputBufHdrs[i]->nInputPortIndex = 0;
    pInputBufHdrs[i]->nFlags = 0;
//pBufHdr[bufCnt]->pAppPrivate = this;
    DEBUG_PRINT("%s: Timestamp sent(%lld)", __FUNCTION__, pInputBufHdrs[i]->nTimeStamp);
    ret = OMX_EmptyThisBuffer(dec_handle, pInputBufHdrs[i]);
    if (OMX_ErrorNone != ret) {
      DEBUG_PRINT_ERROR("ERROR - OMX_EmptyThisBuffer failed with result %d", ret);
      do_freeHandle_and_clean_up(true);
      return -1;
    }
    else {
      DEBUG_PRINT("OMX_EmptyThisBuffer success!");
      etb_count++;
    }
  }

  if(0 != pthread_create(&ebd_thread_id, NULL, ebd_thread, NULL))
  {
    DEBUG_PRINT_ERROR(" Error in Creating fbd_thread ");
    free_queue(ebd_queue);
    free_queue(fbd_queue);
    return -1;
  }

  // wait for event port settings changed event
  DEBUG_PRINT("wait_for_event: dyn reconfig");
  wait_for_event();
  DEBUG_PRINT("wait_for_event: dyn reconfig rcvd, currentStatus %d",
             currentStatus);
  if (currentStatus == ERROR_STATE)
  {
    DEBUG_PRINT_ERROR("Error - ERROR_STATE");
    do_freeHandle_and_clean_up(true);
    return -1;
  }
  else if (currentStatus == PORT_SETTING_CHANGE_STATE)
  {
    if (output_port_reconfig() != 0)
      return -1;
  }

  if (freeHandle_option == FREE_HANDLE_AT_EXECUTING)
  {
    OMX_STATETYPE state = OMX_StateInvalid;
    OMX_GetState(dec_handle, &state);
    if (state == OMX_StateExecuting)
    {
      DEBUG_PRINT("Decoder is in OMX_StateExecuting and trying to call OMX_FreeHandle ");
      do_freeHandle_and_clean_up(false);
    }
    else
    {
      DEBUG_PRINT_ERROR("Error - Decoder is in state %d and trying to call OMX_FreeHandle ", state);
      do_freeHandle_and_clean_up(true);
    }
    return -1;
  }
  else if (freeHandle_option == FREE_HANDLE_AT_PAUSE)
  {
    OMX_SendCommand(dec_handle, OMX_CommandStateSet, OMX_StatePause,0);
    wait_for_event();

    OMX_STATETYPE state = OMX_StateInvalid;
    OMX_GetState(dec_handle, &state);
    if (state == OMX_StatePause)
    {
      DEBUG_PRINT("Decoder is in OMX_StatePause and trying to call OMX_FreeHandle ");
      do_freeHandle_and_clean_up(false);
    }
    else
    {
      DEBUG_PRINT_ERROR("Error - Decoder is in state %d and trying to call OMX_FreeHandle ", state);
      do_freeHandle_and_clean_up(true);
    }
    return -1;
  }

  return 0;
}

static OMX_ERRORTYPE Allocate_Buffer ( OMX_COMPONENTTYPE *dec_handle,
                                       OMX_BUFFERHEADERTYPE  ***pBufHdrs,
                                       OMX_U32 nPortIndex,
                                       long bufCntMin, long bufSize)
{
  DEBUG_PRINT("Inside %s ", __FUNCTION__);
  OMX_ERRORTYPE error=OMX_ErrorNone;
  long bufCnt=0;

  DEBUG_PRINT("pBufHdrs = %x,bufCntMin = %d", pBufHdrs, bufCntMin);
  *pBufHdrs= (OMX_BUFFERHEADERTYPE **)
      malloc(sizeof(OMX_BUFFERHEADERTYPE *) * bufCntMin);

  for(bufCnt=0; bufCnt < bufCntMin; ++bufCnt) {
    DEBUG_PRINT("OMX_AllocateBuffer No %d ", bufCnt);
    error = OMX_AllocateBuffer(dec_handle, &((*pBufHdrs)[bufCnt]),
          nPortIndex, NULL, bufSize);
  }

  return error;
}

static void do_freeHandle_and_clean_up(bool isDueToError)
{
  int bufCnt = 0;
  OMX_STATETYPE state = OMX_StateInvalid;
  OMX_GetState(dec_handle, &state);
  if (state == OMX_StateExecuting || state == OMX_StatePause)
  {
    DEBUG_PRINT("Requesting transition to Idle");
    OMX_SendCommand(dec_handle, OMX_CommandStateSet, OMX_StateIdle, 0);
    wait_for_event();
  }
  OMX_GetState(dec_handle, &state);
  if (state == OMX_StateIdle)
  {
    DEBUG_PRINT("Requesting transition to Loaded");
    OMX_SendCommand(dec_handle, OMX_CommandStateSet, OMX_StateLoaded, 0);
    for(bufCnt=0; bufCnt < input_buf_cnt; ++bufCnt)
    {
      OMX_FreeBuffer(dec_handle, 0, pInputBufHdrs[bufCnt]);
    }
    if (pInputBufHdrs)
    {
      free(pInputBufHdrs);
      pInputBufHdrs = NULL;
    }
    for(bufCnt = 0; bufCnt < portFmt.nBufferCountActual; ++bufCnt) {
      OMX_FreeBuffer(dec_handle, 1, pOutYUVBufHdrs[bufCnt]);
    }
    wait_for_event();
  }

  DEBUG_PRINT("[OMX Vdec Test] - Free handle decoder\n");
  OMX_ERRORTYPE result = OMX_FreeHandle(dec_handle);
  if (result != OMX_ErrorNone)
  {
    DEBUG_PRINT_ERROR("[OMX Vdec Test] - OMX_FreeHandle error. Error code: %d", result);
  }
  dec_handle = NULL;

  /* Deinit OpenMAX */
  DEBUG_PRINT("[OMX Vdec Test] - De-initializing OMX ");
  OMX_Deinit();

  DEBUG_PRINT("[OMX Vdec Test] - closing all files");
  if(inputBufferFileFd != -1)
  {
    close(inputBufferFileFd);
    inputBufferFileFd = -1;
  }

  DEBUG_PRINT("[OMX Vdec Test] - after free inputfile");

  if (takeYuvLog && outputBufferFile) {
    fclose(outputBufferFile);
    outputBufferFile = NULL;
  }
  DEBUG_PRINT("[OMX Vdec Test] - after free outputfile");

  if(ebd_queue)
  {
    free_queue(ebd_queue);
    ebd_queue = NULL;
  }
  DEBUG_PRINT("[OMX Vdec Test] - after free ebd_queue ");
  if(fbd_queue)
  {
    free_queue(fbd_queue);
    fbd_queue = NULL;
  }
  DEBUG_PRINT("[OMX Vdec Test] - after free iftb_queue");
  printf("*****************************************\n");
  if (isDueToError)
    printf("************...TEST FAILED...************\n");
  else
    printf("**********...TEST SUCCESSFULL...*********\n");
  printf("*****************************************\n");
}

static int Read_Buffer_From_DAT_File(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  long frameSize=0;
  char temp_buffer[10];
  char temp_byte;
  int bytes_read=0;
  int i=0;
  unsigned char *read_buffer=NULL;
  char c = '1'; //initialize to anything except '\0'(0)
  char inputFrameSize[12];
  int count =0; char cnt =0;
  memset(temp_buffer, 0, sizeof(temp_buffer));

  DEBUG_PRINT("Inside %s ", __FUNCTION__);

  while (cnt < 10)
  /* Check the input file format, may result in infinite loop */
  {
    DEBUG_PRINT("loop[%d] count[%d]",cnt,count);
    count = read( inputBufferFileFd, &inputFrameSize[cnt], 1);
    if(inputFrameSize[cnt] == '\0' )
      break;
      cnt++;
  }
  inputFrameSize[cnt]='\0';
  frameSize = atoi(inputFrameSize);
  pBufHdr->nFilledLen = 0;

  /* get the frame length */
  lseek64(inputBufferFileFd, -1, SEEK_CUR);
  bytes_read = read(inputBufferFileFd, pBufHdr->pBuffer, frameSize);

  DEBUG_PRINT("Actual frame Size [%d] bytes_read using fread[%d]",
                  frameSize, bytes_read);

  if(bytes_read == 0 || bytes_read < frameSize ) {
    DEBUG_PRINT("Bytes read Zero After Read frame Size ");
    DEBUG_PRINT("Checking VideoPlayback Count:video_playback_count is:%d",
                       video_playback_count);
    return 0;
  }
  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;
  return bytes_read;
}

static int Read_Buffer_From_H264_Start_Code_File(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  int bytes_read = 0;
  int cnt = 0;
  unsigned int code = 0;
  int naluType = 0;
  int newFrame = 0;
  char *dataptr = (char *)pBufHdr->pBuffer;
  DEBUG_PRINT("Inside %s", __FUNCTION__);
  do
  {
    newFrame = 0;
    bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
    if (!bytes_read)
    {
      DEBUG_PRINT("%s: Bytes read Zero", __FUNCTION__);
      break;
    }
    code <<= 8;
    code |= (0x000000FF & dataptr[cnt]);
    cnt++;
    if ((cnt == 4) && (code != H264_START_CODE))
    {
      DEBUG_PRINT_ERROR("%s: ERROR: Invalid start code found 0x%x", __FUNCTION__, code);
      cnt = 0;
      break;
    }
    if ((cnt > 4) && (code == H264_START_CODE))
    {
      DEBUG_PRINT("%s: Found H264_START_CODE", __FUNCTION__);
      bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
      if (!bytes_read)
      {
        DEBUG_PRINT("%s: Bytes read Zero", __FUNCTION__);
        break;
      }
      DEBUG_PRINT("%s: READ Byte[%d] = 0x%x", __FUNCTION__, cnt, dataptr[cnt]);
      naluType = dataptr[cnt] & 0x1F;
      cnt++;
      if ((naluType == 1) || (naluType == 5))
      {
        DEBUG_PRINT("%s: Found AU", __FUNCTION__);
        bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
        if (!bytes_read)
        {
          DEBUG_PRINT("%s: Bytes read Zero", __FUNCTION__);
          break;
        }
        DEBUG_PRINT("%s: READ Byte[%d] = 0x%x", __FUNCTION__, cnt, dataptr[cnt]);
        newFrame = (dataptr[cnt] & 0x80);
        cnt++;
        if (newFrame)
        {
          lseek64(inputBufferFileFd, -6, SEEK_CUR);
          cnt -= 6;
          DEBUG_PRINT("%s: Found a NAL unit (type 0x%x) of size = %d", __FUNCTION__, (dataptr[4] & 0x1F), cnt);
          break;
        }
        else
        {
          DEBUG_PRINT("%s: Not a New Frame", __FUNCTION__);
        }
      }
      else
      {
        lseek64(inputBufferFileFd, -5, SEEK_CUR);
        cnt -= 5;
        DEBUG_PRINT("%s: Found NAL unit (type 0x%x) of size = %d", __FUNCTION__, (dataptr[4] & 0x1F), cnt);
        break;
      }
    }
  } while (1);
  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;
  return cnt;
}
static int Read_Buffer_From_H265_Start_Code_File(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  int bytes_read = 0;
  int cnt = 0;
  unsigned int code = 0;
  int naluType = 0;
  int newFrame = 0;
  char *dataptr = (char *)pBufHdr->pBuffer;
  DEBUG_PRINT("Inside %s", __FUNCTION__);
  do
  {
    newFrame = 0;
    bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
    if (!bytes_read)
    {
      DEBUG_PRINT("%s: Bytes read Zero", __FUNCTION__);
      break;
    }
    code <<= 8;
    code |= (0x000000FF & dataptr[cnt]);
    cnt++;
    if ((cnt == 4) && (code != H265_START_CODE))
    {
      DEBUG_PRINT_ERROR("%s: ERROR: Invalid start code found 0x%x", __FUNCTION__, code);
      cnt = 0;
      break;
    }
    if ((cnt > 4) && (code == H265_START_CODE))
    {
      DEBUG_PRINT("%s: Found H265_START_CODE", __FUNCTION__);
      bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
      if (!bytes_read)
      {
        DEBUG_PRINT("%s: Bytes read Zero", __FUNCTION__);
        break;
      }
      DEBUG_PRINT("%s: READ Byte[%d] = 0x%x", __FUNCTION__, cnt, dataptr[cnt]);
      naluType = (dataptr[cnt] & HEVC_NAL_UNIT_TYPE_MASK) >> 1;
      cnt++;
      if (naluType <= HEVC_NAL_UNIT_TYPE_VCL_LIMIT &&
          naluType != HEVC_NAL_UNIT_TYPE_RSV_IRAP_VCL22 &&
          naluType != HEVC_NAL_UNIT_TYPE_RSV_IRAP_VCL23 &&
          naluType != HEVC_NAL_UNIT_TYPE_RSV_VCL_N10 &&
          naluType != HEVC_NAL_UNIT_TYPE_RSV_VCL_N12 &&
          naluType != HEVC_NAL_UNIT_TYPE_RSV_VCL_N14 &&
          naluType != HEVC_NAL_UNIT_TYPE_RSV_VCL_R11 &&
          naluType != HEVC_NAL_UNIT_TYPE_RSV_VCL_R13 &&
          naluType != HEVC_NAL_UNIT_TYPE_RSV_VCL_R15)
      {
        DEBUG_PRINT("%s: Found AU for HEVC \n", __FUNCTION__);
        bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
        if (!bytes_read)
        {
          DEBUG_PRINT("%s: Bytes read Zero", __FUNCTION__);
          break;
        }
        cnt ++;
        bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
        DEBUG_PRINT("%s: READ Byte[%d] = 0x%x firstsliceflag %d\n", __FUNCTION__, cnt, dataptr[cnt],dataptr[cnt] >>7);
        newFrame = (dataptr[cnt] >>7); //firstsliceflag
        cnt++;
        if (newFrame)
        {
          lseek64(inputBufferFileFd, -7, SEEK_CUR);
          cnt -= 7;
          DEBUG_PRINT("%s: Found a NAL unit (type 0x%x) of size = %d", __FUNCTION__, ((dataptr[4] & HEVC_NAL_UNIT_TYPE_MASK) >> 1), cnt);
          break;
        }
        else
        {
          DEBUG_PRINT("%s: Not a New Frame", __FUNCTION__);
        }
      }
      else
      {
        lseek64(inputBufferFileFd, -5, SEEK_CUR);
        cnt -= 5;
        DEBUG_PRINT("%s: Found NAL unit (type 0x%x) of size = %d", __FUNCTION__, ((dataptr[4] & HEVC_NAL_UNIT_TYPE_MASK) >> 1), cnt);
        break;
      }
    }
  } while (1);
  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;
  return cnt;
}

static int Read_Buffer_ArbitraryBytes(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  int bytes_read=0;
  DEBUG_PRINT("Inside %s ", __FUNCTION__);
  bytes_read = read(inputBufferFileFd, pBufHdr->pBuffer, NUMBER_OF_ARBITRARYBYTES_READ);
  if(bytes_read == 0) {
    DEBUG_PRINT("Bytes read Zero After Read frame Size ");
    DEBUG_PRINT("Checking VideoPlayback Count:video_playback_count is:%d",
                  video_playback_count);
    return 0;
  }
  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;
  return bytes_read;
}

static int Read_Buffer_From_Vop_Start_Code_File(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  unsigned int readOffset = 0;
  int bytes_read = 0;
  unsigned int code = 0;
  pBufHdr->nFilledLen = 0;
  static unsigned int header_code = 0;

  DEBUG_PRINT("Inside %s", __FUNCTION__);

  do
  {
    //Start codes are always byte aligned.
    bytes_read = read(inputBufferFileFd, &pBufHdr->pBuffer[readOffset], 1);
    if(bytes_read == 0 || bytes_read == -1)
    {
      DEBUG_PRINT("Bytes read Zero ");
      break;
    }
    code <<= 8;
    code |= (0x000000FF & pBufHdr->pBuffer[readOffset]);
    //VOP start code comparision
    if (readOffset>3)
    {
      if(!header_code ){
        if( VOP_START_CODE == code)
        {
          header_code = VOP_START_CODE;
        }
        else if ( (0xFFFFFC00 & code) == SHORT_HEADER_START_CODE )
        {
          header_code = SHORT_HEADER_START_CODE;
        }
      }
      if ((header_code == VOP_START_CODE) && (code == VOP_START_CODE))
      {
        //Seek backwards by 4
        lseek64(inputBufferFileFd, -4, SEEK_CUR);
        readOffset-=3;
        break;
      }
      else if (( header_code == SHORT_HEADER_START_CODE ) && ( SHORT_HEADER_START_CODE == (code & 0xFFFFFC00)))
      {
        //Seek backwards by 4
        lseek64(inputBufferFileFd, -4, SEEK_CUR);
        readOffset-=3;
        break;
      }
    }
    readOffset++;
  }while (1);
  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;
  return readOffset;
}
static int Read_Buffer_From_Mpeg2_Start_Code(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  unsigned int readOffset = 0;
  int bytesRead = 0;
  unsigned int code = 0;
  pBufHdr->nFilledLen = 0;
  static unsigned int firstParse = true;
  unsigned int seenFrame = false;

  DEBUG_PRINT("Inside %s", __FUNCTION__);

  /* Read one byte at a time. Construct the code every byte in order to
   * compare to the start codes. Keep looping until we've read in a complete
   * frame, which can be either just a picture start code + picture, or can
   * include the sequence header as well
   */
  while (1) {
    bytesRead = read(inputBufferFileFd, &pBufHdr->pBuffer[readOffset], 1);

    /* Exit the loop if we can't read any more bytes */
    if (bytesRead == 0 || bytesRead == -1) {
      break;
    }

    /* Construct the code one byte at a time */
    code <<= 8;
    code |= (0x000000FF & pBufHdr->pBuffer[readOffset]);

    /* Can't compare the code to MPEG2 start codes until we've read the
     * first four bytes
     */
    if (readOffset >= 3) {

      /* If this is the first time we're reading from the file, then we
       * need to throw away the system start code information at the
       * beginning. We can just look for the first sequence header.
       */
      if (firstParse) {
        if (code == MPEG2_SEQ_START_CODE) {
          /* Seek back by 4 bytes and reset code so that we can skip
           * down to the common case below.
           */
          lseek(inputBufferFileFd, -4, SEEK_CUR);
          code = 0;
          readOffset -= 3;
          firstParse = false;
          continue;
        }
      }

      /* If we have already parsed a frame and we see a sequence header, then
       * the sequence header is part of the next frame so we seek back and
       * break.
       */
      if (code == MPEG2_SEQ_START_CODE) {
        if (seenFrame) {
          lseek(inputBufferFileFd, -4, SEEK_CUR);
          readOffset -= 3;
          break;
        }
        /* If we haven't seen a frame yet, then read in all the data until we
         * either see another frame start code or sequence header start code.
         */
      } else if (code == MPEG2_FRAME_START_CODE) {
        if (!seenFrame) {
          seenFrame = true;
        } else {
          lseek(inputBufferFileFd, -4, SEEK_CUR);
          readOffset -= 3;
          break;
        }
      }
    }

    readOffset++;
  }

  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;
  return readOffset;
}


static int Read_Buffer_From_Size_Nal(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  // NAL unit stream processing
  char temp_size[SIZE_NAL_FIELD_MAX];
  int i = 0;
  int j = 0;
  unsigned int size = 0;   // Need to make sure that uint32 has SIZE_NAL_FIELD_MAX (4) bytes
  int bytes_read = 0;

  // read the "size_nal_field"-byte size field
  bytes_read = read(inputBufferFileFd, pBufHdr->pBuffer + pBufHdr->nOffset, nalSize);
  if (bytes_read == 0 || bytes_read == -1)
  {
    DEBUG_PRINT("Failed to read frame or it might be EOF");
    return 0;
  }

  for (i=0; i<SIZE_NAL_FIELD_MAX-nalSize; i++)
  {
    temp_size[SIZE_NAL_FIELD_MAX - 1 - i] = 0;
  }

  /* Due to little endiannes, Reorder the size based on size_nal_field */
  for (j=0; i<SIZE_NAL_FIELD_MAX; i++, j++)
  {
    temp_size[SIZE_NAL_FIELD_MAX - 1 - i] = pBufHdr->pBuffer[pBufHdr->nOffset + j];
  }
  size = (unsigned int)(*((unsigned int *)(temp_size)));

  // now read the data
  bytes_read = read(inputBufferFileFd, pBufHdr->pBuffer + pBufHdr->nOffset + nalSize, size);
  if (bytes_read != size)
  {
    DEBUG_PRINT_ERROR("Failed to read frame");
  }

  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;

  return bytes_read + nalSize;
}

static int Read_Buffer_From_RCV_File_Seq_Layer(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  unsigned int readOffset = 0, size_struct_C = 0;
  unsigned int startcode = 0;
  pBufHdr->nFilledLen = 0;
  pBufHdr->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;

  DEBUG_PRINT("Inside %s ", __FUNCTION__);

  if (read(inputBufferFileFd, &startcode, 4) <= 0)
    DEBUG_PRINT_ERROR("Error while reading");

  /* read size of struct C as it need not be 4 always*/
  if (read(inputBufferFileFd, &size_struct_C, 4) <= 0)
    DEBUG_PRINT_ERROR("Error while reading");

  if ((startcode & 0xFF000000) == 0xC5000000)
  {

    DEBUG_PRINT("Read_Buffer_From_RCV_File_Seq_Layer size_struct_C: %d", size_struct_C);
    readOffset = read(inputBufferFileFd, pBufHdr->pBuffer, size_struct_C);
    lseek64(inputBufferFileFd, 24, SEEK_CUR);
  }
  else if((startcode & 0xFF000000) == 0x85000000)
  {
    // .RCV V1 file

    rcv_v1 = 1;

    DEBUG_PRINT("Read_Buffer_From_RCV_File_Seq_Layer size_struct_C: %d", size_struct_C);
    readOffset = read(inputBufferFileFd, pBufHdr->pBuffer, size_struct_C);
    lseek64(inputBufferFileFd, 8, SEEK_CUR);

  }
  else
  {
    DEBUG_PRINT_ERROR("Error: Unknown VC1 clip format %x", startcode);
  }

#if 0
    {
      int i=0;
      DEBUG_PRINT("Read_Buffer_From_RCV_File, length %d readOffset %d", readOffset, readOffset);
      for (i=0; i<36; i++)
      {
        DEBUG_PRINT("0x%.2x ", pBufHdr->pBuffer[i]);
        if (i%16 == 15) {
          printf("\n");
        }
      }
      printf("\n");
    }
#endif
  return readOffset;
}

static int Read_Buffer_From_RCV_File(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  unsigned int readOffset = 0;
  unsigned int len = 0;
  unsigned int key = 0;
  DEBUG_PRINT("Inside %s ", __FUNCTION__);

  DEBUG_PRINT("Read_Buffer_From_RCV_File - nOffset %d", pBufHdr->nOffset);
  if(rcv_v1)
  {
    /* for the case of RCV V1 format, the frame header is only of 4 bytes and has
       only the frame size information */
    readOffset = read(inputBufferFileFd, &len, 4);
    DEBUG_PRINT("Read_Buffer_From_RCV_File - framesize %d %x", len, len);

  }
  else
  {
    /* for a regular RCV file, 3 bytes comprise the frame size and 1 byte for key*/
    readOffset = read(inputBufferFileFd, &len, 3);
    DEBUG_PRINT("Read_Buffer_From_RCV_File - framesize %d %x", len, len);

    readOffset = read(inputBufferFileFd, &key, 1);
    if ( (key & 0x80) == false)
    {
      DEBUG_PRINT("Read_Buffer_From_RCV_File - Non IDR frame key %x", key);
    }

  }

  if(!rcv_v1)
  {
    /* There is timestamp field only for regular RCV format and not for RCV V1 format*/
    readOffset = read(inputBufferFileFd, &pBufHdr->nTimeStamp, 4);
    DEBUG_PRINT("Read_Buffer_From_RCV_File - timeStamp %d", pBufHdr->nTimeStamp);
    pBufHdr->nTimeStamp *= 1000;
  }
  else
  {
    pBufHdr->nTimeStamp = timeStampLfile;
    timeStampLfile += timestampInterval;
  }

  if(len > pBufHdr->nAllocLen)
  {
    DEBUG_PRINT_ERROR("Error in sufficient buffer framesize %d, allocalen %d noffset %d",len,pBufHdr->nAllocLen, pBufHdr->nOffset);
    readOffset = read(inputBufferFileFd, pBufHdr->pBuffer+pBufHdr->nOffset,
                     pBufHdr->nAllocLen - pBufHdr->nOffset);

    loff_t off = (len - readOffset)*1LL;
    lseek64(inputBufferFileFd, off ,SEEK_CUR);
    return readOffset;
  }
  else {
    readOffset = read(inputBufferFileFd, pBufHdr->pBuffer+pBufHdr->nOffset, len);
  }
  if (readOffset != len)
  {
    DEBUG_PRINT("EOS reach or Reading error %d, %s ", readOffset, strerror( errno ));
    return 0;
  }

#if 0
    {
      int i=0;
      DEBUG_PRINT("Read_Buffer_From_RCV_File, length %d readOffset %d", len, readOffset);
      for (i=0; i<64; i++)
      {
        DEBUG_PRINT("0x%.2x ", pBufHdr->pBuffer[i]);
        if (i%16 == 15) {
          printf("\n");
        }
      }
      printf("\n");
    }
#endif

  return readOffset;
}

static int Read_Buffer_From_VC1_File(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  static int timeStampLfile = 0;
  OMX_U8 *pBuffer = pBufHdr->pBuffer + pBufHdr->nOffset;
  DEBUG_PRINT("Inside %s ", __FUNCTION__);
  unsigned int readOffset = 0;
  int bytes_read = 0;
  unsigned int code = 0, total_bytes = 0;
  int startCode_cnt = 0;
  int bSEQflag = 0;
  int bEntryflag = 0;
  unsigned int SEQbytes = 0;
  int numStartcodes = 0;

  numStartcodes = bHdrflag?1:2;

  do
  {
    if (total_bytes == pBufHdr->nAllocLen)
    {
      DEBUG_PRINT_ERROR("Buffer overflow!");
      break;
    }
    //Start codes are always byte aligned.
    bytes_read = read(inputBufferFileFd, &pBuffer[readOffset],1 );

    if(!bytes_read)
    {
      DEBUG_PRINT(" Bytes read Zero ");
      break;
    }
    total_bytes++;
    code <<= 8;
    code |= (0x000000FF & pBufHdr->pBuffer[readOffset]);

    if(!bSEQflag && (code == VC1_SEQUENCE_START_CODE)) {
      if(startCode_cnt) bSEQflag = 1;
    }

    if(!bEntryflag && ( code == VC1_ENTRY_POINT_START_CODE)) {
      if(startCode_cnt) bEntryflag = 1;
    }

    if(code == VC1_FRAME_START_CODE || code == VC1_FRAME_FIELD_CODE)
    {
      startCode_cnt++ ;
    }

    //VOP start code comparision
    if(startCode_cnt == numStartcodes)
    {
      if (VC1_FRAME_START_CODE == (code & 0xFFFFFFFF) ||
        VC1_FRAME_FIELD_CODE == (code & 0xFFFFFFFF))
      {
        previous_vc1_au = 0;
        if(VC1_FRAME_FIELD_CODE == (code & 0xFFFFFFFF))
        {
          previous_vc1_au = 1;
        }

        if(!bHdrflag && (bSEQflag || bEntryflag)) {
          lseek(inputBufferFileFd,-(SEQbytes+4),SEEK_CUR);
          readOffset -= (SEQbytes+3);
        }
        else {
          //Seek backwards by 4
          lseek64(inputBufferFileFd, -4, SEEK_CUR);
          readOffset-=3;
        }

        while(pBufHdr->pBuffer[readOffset-1] == 0)
         readOffset--;

        break;
      }
    }
    readOffset++;
    if(bSEQflag || bEntryflag) {
      SEQbytes++;
    }
  }while (1);

  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;

#if 0
    {
      int i=0;
      DEBUG_PRINT("Read_Buffer_From_VC1_File, readOffset %d", readOffset);
      for (i=0; i<64; i++)
      {
        DEBUG_PRINT("0x%.2x ", pBufHdr->pBuffer[i]);
        if (i%16 == 15) {
          printf("\n");
        }
      }
      printf("\n");
    }
#endif

  return readOffset;
}

static int Read_Buffer_From_DivX_4_5_6_File(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
#define MAX_NO_B_FRMS 3 // Number of non-b-frames packed in each buffer
#define N_PREV_FRMS_B 1 // Number of previous non-b-frames packed
                        // with a set of consecutive b-frames
#define FRM_ARRAY_SIZE (MAX_NO_B_FRMS + N_PREV_FRMS_B)
  char *p_buffer = NULL;
  unsigned int offset_array[FRM_ARRAY_SIZE];
  int byte_cntr, pckt_end_idx;
  unsigned int read_code = 0, bytes_read, byte_pos = 0, frame_type;
  unsigned int i, b_frm_idx, b_frames_found = 0, vop_set_cntr = 0;
  bool pckt_ready = false;
#ifdef __DEBUG_DIVX__
  char pckt_type[20];
  int pckd_frms = 0;
  static unsigned long long int total_bytes = 0;
  static unsigned long long int total_frames = 0;
#endif //__DEBUG_DIVX__

  DEBUG_PRINT("Inside %s ", __FUNCTION__);

  do {
    p_buffer = (char *)pBufHdr->pBuffer + byte_pos;

    bytes_read = read(inputBufferFileFd, p_buffer, NUMBER_OF_ARBITRARYBYTES_READ);
    byte_pos += bytes_read;
    for (byte_cntr = 0; byte_cntr < bytes_read && !pckt_ready; byte_cntr++) {
      read_code <<= 8;
      ((char*)&read_code)[0] = p_buffer[byte_cntr];
      if (read_code == VOP_START_CODE) {
        if (++byte_cntr < bytes_read) {
          frame_type = p_buffer[byte_cntr];
          frame_type &= 0x000000C0;
#ifdef __DEBUG_DIVX__
          switch (frame_type) {
            case 0x00: pckt_type[pckd_frms] = 'I'; break;
            case 0x40: pckt_type[pckd_frms] = 'P'; break;
            case 0x80: pckt_type[pckd_frms] = 'B'; break;
            default: pckt_type[pckd_frms] = 'X';
          }
          pckd_frms++;
#endif // __DEBUG_DIVX__
          offset_array[vop_set_cntr] = byte_pos - bytes_read + byte_cntr - 4;
          if (frame_type == 0x80) { // B Frame found!
            if (!b_frames_found) {
              // Try to packet N_PREV_FRMS_B previous frames
              // with the next consecutive B frames
              i = N_PREV_FRMS_B;
              while ((vop_set_cntr - i) < 0 && i > 0) i--;
              b_frm_idx = vop_set_cntr - i;
              if (b_frm_idx > 0) {
                pckt_end_idx = b_frm_idx;
                pckt_ready = true;
#ifdef __DEBUG_DIVX__
                pckt_type[b_frm_idx] = '\0';
                total_frames += b_frm_idx;
#endif //__DEBUG_DIVX__
              }
            }
            b_frames_found++;
          } else if (b_frames_found) {
            pckt_end_idx = vop_set_cntr;
            pckt_ready = true;
#ifdef __DEBUG_DIVX__
            pckt_type[pckd_frms - 1] = '\0';
            total_frames += pckd_frms - 1;
#endif //__DEBUG_DIVX__
          } else if (vop_set_cntr == (FRM_ARRAY_SIZE -1)) {
            pckt_end_idx = MAX_NO_B_FRMS;
            pckt_ready = true;
#ifdef __DEBUG_DIVX__
            pckt_type[pckt_end_idx] = '\0';
            total_frames += pckt_end_idx;
#endif //__DEBUG_DIVX__
          } else
            vop_set_cntr++;
        } else {
          // The vop start code was found in the last 4 bytes,
          // seek backwards by 4 to include this start code
          // with the next buffer.
          lseek64(inputBufferFileFd, -4, SEEK_CUR);
          byte_pos -= 4;
#ifdef __DEBUG_DIVX__
          pckd_frms--;
#endif //__DEBUG_DIVX__
        }
      }
    }
    if (pckt_ready) {
      loff_t off = (byte_pos - offset_array[pckt_end_idx]);
      if ( lseek64(inputBufferFileFd, -1LL*off , SEEK_CUR) == -1 ){
        DEBUG_PRINT_ERROR("lseek64 with offset = %lld failed with errno %d"
                       ", current position =0x%llx", -1LL*off,
                       errno, lseek64(inputBufferFileFd, 0, SEEK_CUR));
      }
    }
    else {
      char eofByte;
      int ret = read(inputBufferFileFd, &eofByte, 1 );
      if ( ret == 0 ) {
        offset_array[vop_set_cntr] = byte_pos;
        pckt_end_idx = vop_set_cntr;
        pckt_ready = true;
#ifdef __DEBUG_DIVX__
        pckt_type[pckd_frms] = '\0';
        total_frames += pckd_frms;
#endif //__DEBUG_DIVX__
      }
      else if (ret == 1){
        if ( lseek64(inputBufferFileFd, -1, SEEK_CUR ) == -1 ){
          DEBUG_PRINT_ERROR("lseek64 failed with errno = %d, "
                           "current fileposition = %llx",
                            errno,
                            lseek64(inputBufferFileFd, 0, SEEK_CUR));
        }
      }
      else {
        DEBUG_PRINT_ERROR("Error when checking for EOF");
      }
    }
  } while (!pckt_ready);
  pBufHdr->nFilledLen = offset_array[pckt_end_idx];
  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;
#ifdef __DEBUG_DIVX__
  total_bytes += pBufHdr->nFilledLen;
  ALOGE("[DivX] Packet: Type[%s] Size[%u] TS[%lld] TB[%llx] NFrms[%lld]",
    pckt_type, pBufHdr->nFilledLen, pBufHdr->nTimeStamp,
    total_bytes, total_frames);
#endif //__DEBUG_DIVX__
  return pBufHdr->nFilledLen;
}

static int Read_Buffer_From_DivX_311_File(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  static OMX_S64 timeStampLfile = 0;
  char *p_buffer = NULL;
  bool pkt_ready = false;
  unsigned int frame_type = 0;
  unsigned int bytes_read = 0;
  unsigned int frame_size = 0;
  unsigned int num_bytes_size = 4;
  unsigned int num_bytes_frame_type = 1;
  unsigned int n_offset = 0;

  if (pBufHdr != NULL)
  {
    p_buffer = (char *)pBufHdr->pBuffer + pBufHdr->nOffset;
  }
  else
  {
    DEBUG_PRINT(" ERROR:Read_Buffer_From_DivX_311_File: pBufHdr is NULL");
    return 0;
  }

  n_offset = pBufHdr->nOffset;

  DEBUG_PRINT("Inside %s ", __FUNCTION__);

  pBufHdr->nTimeStamp = timeStampLfile;

  if (p_buffer == NULL)
  {
    DEBUG_PRINT(" ERROR:Read_Buffer_From_DivX_311_File: p_bufhdr is NULL");
    return 0;
  }

  //Read first frame based on size
  //DivX 311 frame - 4 byte header with size followed by the frame

  bytes_read = read(inputBufferFileFd, &frame_size, num_bytes_size);

  DEBUG_PRINT("Read_Buffer_From_DivX_311_File: Frame size = %d", frame_size);
  n_offset += read(inputBufferFileFd, p_buffer, frame_size);

  pBufHdr->nTimeStamp = timeStampLfile;

  timeStampLfile += timestampInterval;

  //the packet is ready to be sent
  DEBUG_PRINT("Returning Read Buffer from Divx 311: TS=[%ld], Offset=[%d]",
       (long int)pBufHdr->nTimeStamp,
       n_offset );

  return n_offset;
}
static int Read_Buffer_From_VP8_File(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  static OMX_S64 timeStampLfile = 0;
  char *p_buffer = NULL;
  bool pkt_ready = false;
  unsigned int frame_type = 0;
  unsigned int bytes_read = 0;
  unsigned int frame_size = 0;
  unsigned int num_bytes_size = 4;
  unsigned int num_bytes_frame_type = 1;
  unsigned long long time_stamp;
  unsigned int n_offset = 0;
  static int ivf_header_read;

  if (pBufHdr != NULL)
  {
    p_buffer = (char *)pBufHdr->pBuffer + pBufHdr->nOffset;
  }
  else
  {
    DEBUG_PRINT(" ERROR:Read_Buffer_From_DivX_311_File: pBufHdr is NULL");
    return 0;
  }
  n_offset = pBufHdr->nOffset;

  if (p_buffer == NULL)
  {
    DEBUG_PRINT(" ERROR:Read_Buffer_From_DivX_311_File: p_bufhdr is NULL");
    return 0;
  }

  if(ivf_header_read == 0) {
    bytes_read = read(inputBufferFileFd, p_buffer, 32);
    ivf_header_read = 1;
    if(p_buffer[0] == 'D' && p_buffer[1] == 'K' && p_buffer[2] == 'I' && p_buffer[3] == 'F')
    {
      DEBUG_PRINT("  IVF header found  ");
    } else
    {
      DEBUG_PRINT("  No IVF header found  ");
      lseek(inputBufferFileFd, -32, SEEK_CUR);
    }
  }
  bytes_read = read(inputBufferFileFd, &frame_size, 4);
  bytes_read = read(inputBufferFileFd, &time_stamp, 8);
  n_offset += read(inputBufferFileFd, p_buffer, frame_size);
  pBufHdr->nTimeStamp = time_stamp;
  return n_offset;
}
static int open_video_file ()
{
  int error_code = 0;
  char outputfilename[512];
  DEBUG_PRINT("Inside %s filename=%s", __FUNCTION__, in_filename);

  if ( (inputBufferFileFd = open( in_filename, O_RDONLY | O_LARGEFILE) ) == -1 ){
    DEBUG_PRINT_ERROR("Error - i/p file %s could NOT be opened errno = %d",
                    in_filename, errno);
    error_code = -1;
  }
  else {
    DEBUG_PRINT_ERROR("i/p file %s is opened ", in_filename);
  }

  if (takeYuvLog) {
    strlcpy(outputfilename, "yuvframes.yuv", sizeof(outputfilename));
    outputBufferFile = fopen (outputfilename, "wb");
    if (outputBufferFile == NULL)
    {
      DEBUG_PRINT_ERROR("ERROR - o/p file %s could NOT be opened", outputfilename);
      error_code = -1;
    }
    else
    {
      DEBUG_PRINT("O/p file %s is opened ", outputfilename);
    }
  }

  return error_code;
}

int disable_output_port()
{
  DEBUG_PRINT("DISABLING OP PORT");
  pthread_mutex_lock(&enable_lock);
  sent_disabled = 1;
  // Send DISABLE command
  OMX_SendCommand(dec_handle, OMX_CommandPortDisable, 1, 0);
  pthread_mutex_unlock(&enable_lock);
  // wait for Disable event to come back
  wait_for_event();
  if (currentStatus == ERROR_STATE)
  {
    do_freeHandle_and_clean_up(true);
    return -1;
  }
  DEBUG_PRINT("OP PORT DISABLED!");
  return 0;
}

int enable_output_port()
{
  int bufCnt = 0;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  DEBUG_PRINT("ENABLING OP PORT");
  // Send Enable command
  OMX_SendCommand(dec_handle, OMX_CommandPortEnable, 1, 0);

  /* Allocate buffer on decoder's o/p port */
  portFmt.nPortIndex = 1;

  if (anti_flickering) {
    ret = OMX_GetParameter(dec_handle,OMX_IndexParamPortDefinition,&portFmt);
    if (ret != OMX_ErrorNone) {
      DEBUG_PRINT_ERROR("%s: OMX_GetParameter failed: %d",__FUNCTION__, ret);
      return -1;
    }
    portFmt.nBufferCountActual += 1;
    ret = OMX_SetParameter(dec_handle,OMX_IndexParamPortDefinition,&portFmt);
    if (ret != OMX_ErrorNone) {
      DEBUG_PRINT_ERROR("%s: OMX_SetParameter failed: %d",__FUNCTION__, ret);
      return -1;
    }
  }

  error = Allocate_Buffer(dec_handle, &pOutYUVBufHdrs, portFmt.nPortIndex,
                          portFmt.nBufferCountActual, portFmt.nBufferSize);
  if (error != OMX_ErrorNone) {
    DEBUG_PRINT_ERROR("Error - OMX_AllocateBuffer Output buffer error");
    return -1;
  }
  else
  {
    DEBUG_PRINT("OMX_AllocateBuffer Output buffer success");
    free_op_buf_cnt = portFmt.nBufferCountActual;
  }

  // wait for enable event to come back
  wait_for_event();
  if (currentStatus == ERROR_STATE)
  {
    do_freeHandle_and_clean_up(true);
    return -1;
  }
  if (pOutYUVBufHdrs == NULL)
  {
    DEBUG_PRINT_ERROR("Error - pOutYUVBufHdrs is NULL");
    return -1;
  }
  for(bufCnt=0; bufCnt < portFmt.nBufferCountActual; ++bufCnt) {
    DEBUG_PRINT("OMX_FillThisBuffer on output buf no.%d",bufCnt);
    if (pOutYUVBufHdrs[bufCnt] == NULL)
    {
      DEBUG_PRINT_ERROR("Error - pOutYUVBufHdrs[%d] is NULL", bufCnt);
      return -1;
    }
    pOutYUVBufHdrs[bufCnt]->nOutputPortIndex = 1;
    pOutYUVBufHdrs[bufCnt]->nFlags &= ~OMX_BUFFERFLAG_EOS;
    ret = OMX_FillThisBuffer(dec_handle, pOutYUVBufHdrs[bufCnt]);
    if (OMX_ErrorNone != ret) {
      DEBUG_PRINT_ERROR("ERROR - OMX_FillThisBuffer failed with result %d", ret);
    }
    else
    {
      DEBUG_PRINT("OMX_FillThisBuffer success!");
      free_op_buf_cnt--;
    }
  }
  DEBUG_PRINT("OP PORT ENABLED!");
  return 0;
}

int output_port_reconfig()
{
  DEBUG_PRINT("PORT_SETTING_CHANGE_STATE");
  if (disable_output_port() != 0)
    return -1;

  /* Port for which the Client needs to obtain info */
  portFmt.nPortIndex = 1;
  OMX_GetParameter(dec_handle,OMX_IndexParamPortDefinition,&portFmt);
  DEBUG_PRINT("Min Buffer Count=%d", portFmt.nBufferCountMin);
  DEBUG_PRINT("Buffer Size=%d", portFmt.nBufferSize);
  if(OMX_DirOutput != portFmt.eDir) {
    DEBUG_PRINT_ERROR("Error - Expect Output Port");
    return -1;
  }
  height = portFmt.format.video.nFrameHeight;
  width = portFmt.format.video.nFrameWidth;
  stride = portFmt.format.video.nStride;
  sliceheight = portFmt.format.video.nSliceHeight;
  crop_rect.nWidth = width;
  crop_rect.nHeight = height;

  if (enable_output_port() != 0)
    return -1;
  DEBUG_PRINT("PORT_SETTING_CHANGE DONE!");
  return 0;
}

void free_output_buffers()
{
  DEBUG_PRINT(" pOutYUVBufHdrs %p", pOutYUVBufHdrs);
  OMX_BUFFERHEADERTYPE *pBuffer = (OMX_BUFFERHEADERTYPE *)pop(fbd_queue);
  while (pBuffer) {
    DEBUG_PRINT(" Free output buffer %p", pBuffer);
    OMX_FreeBuffer(dec_handle, 1, pBuffer);
    pBuffer = (OMX_BUFFERHEADERTYPE *)pop(fbd_queue);
  }
}

void getFreePmem()
{
#ifndef USE_ION
  int ret = -1;
  /*Open pmem device and query free pmem*/
  int pmem_fd = open (PMEM_DEVICE,O_RDWR);

  if(pmem_fd < 0) {
    ALOGE("Unable to open pmem device");
    return;
  }
  struct pmem_freespace fs;
  ret = ioctl(pmem_fd, PMEM_GET_FREE_SPACE, &fs);
  if(ret) {
    ALOGE("IOCTL to query pmem free space failed");
    goto freespace_query_failed;
  }
  ALOGE("Available free space %lx largest chunk %lx", fs.total, fs.largest);
freespace_query_failed:
  close(pmem_fd);
#endif
}

#ifdef USE_ION
bool alloc_map_ion_memory(OMX_U32 buffer_size,
              struct vdec_ion *ion_info, int flag)
{
  int dev_fd = -EINVAL;
  int rc = -EINVAL;
  int secure_mode = 0;

  if (!ion_info || buffer_size <= 0) {
    DEBUG_PRINT_ERROR("Invalid arguments to alloc_map_ion_memory");
    return FALSE;
  }
  dev_fd = ion_open();
  if (dev_fd < 0) {
    DEBUG_PRINT_ERROR("opening ion device failed with fd = %d", dev_fd);
    return FALSE;
  }
  ion_info->alloc_data.flags = flag;
  ion_info->alloc_data.len = buffer_size;

  ion_info->alloc_data.heap_id_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);
  if (secure_mode && (ion_info->alloc_data.flags & ION_FLAG_SECURE)) {
    ion_info->alloc_data.heap_id_mask = ION_HEAP(MEM_HEAP_ID);
  }
      /* Use secure display cma heap for obvious reasons. */
  if (ion_info->alloc_data.flags & ION_FLAG_CP_BITSTREAM) {
    ion_info->alloc_data.heap_id_mask |= ION_HEAP(ION_SECURE_DISPLAY_HEAP_ID);
  }
  rc = ion_alloc_fd(dev_fd, ion_info->alloc_data.len, 0,
                      ion_info->alloc_data.heap_id_mask, ion_info->alloc_data.flags,
                      &ion_info->data_fd);

  if (rc || ion_info->data_fd < 0) {
    DEBUG_PRINT_ERROR("ION ALLOC memory failed");
    ion_close(ion_info->dev_fd);
    ion_info->data_fd = -1;
    ion_info->dev_fd = -1;
    return FALSE;
  }
  ion_info->dev_fd = dev_fd;

  return TRUE;
}

void free_ion_memory(struct vdec_ion *buf_ion_info) {

  if(!buf_ion_info) {
    DEBUG_PRINT_ERROR(" ION: free called with invalid fd/allocdata");
    return;
  }
  if (buf_ion_info->data_fd >= 0) {
    close(buf_ion_info->data_fd);
    buf_ion_info->data_fd = -1;
  }
  if (buf_ion_info->dev_fd >= 0) {
    ion_close(buf_ion_info->dev_fd);
    buf_ion_info->dev_fd = -1;
  }
}
#endif
