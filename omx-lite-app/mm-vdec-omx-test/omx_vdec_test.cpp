/*--------------------------------------------------------------------------
Copyright (c) 2010 - 2013, 2016 - 2021, The Linux Foundation. All rights reserved.

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
    An Openmax IL test lite application ....
*/

/*
 * OpenMAX call sequence for secure decoding:
 * 1. OMX_Init() // Initialize OpenMAX Core.
 * 2. OMX_GetHandle() // Append ".secure" to component name to initialize it
 *    as secure decoding, and set OMX_CALLBACKTYPE to OMX for client to handle
 *    OMX events. If this call succeeds, then enter OMX_StateLoaded.
 * 3. OMX_GetParameter(OMX_IndexParamVideoInit) // Get in/out port numbers.
 * 4. OMX_SetParameter(OMX_IndexParamPortDefinition) // Set in port definitions
 *    i.e. OMX_VIDEO_CodingAVC/width/height/fps etc.
 * 5. OMX_SetParameter(OMX_IndexParamVideoPortFormat) // Enumerate to set color
 *    format as like YUV NV12 or NV12_UBWC to output.
 * 6. OMX_SetParameter(OMX_QcomIndexParamVideoDecoderPictureOrder) // Set
 *    decoder output picture order as QOMX_VIDEO_DISPLAY_ORDER.
 * 7. OMX_SetConfig(OMX_IndexConfigVideoNalSize) // set NAL size as 0 since it
 *    only suppurts to read in H.264 or H.265 Start Code File currently.
 * 8. OMX_SendCommand(OMX_CommandStateSet) // Enter OMX_StateIdle.
 * 9. OMX_AllocateBuffer(inPort) // Allocate secure input ION pmem buffers.
 * 10.alloc_nonsecure_buffer(in) // Allocate system memory to read in frame
 *    and copy it into secure input buffer for OMX to do secure decodeing.
 * 11.OMX_AllocateBuffer(outPort) // Allocate secure output GBM pmem buffers.
 * 12.OMX_SendCommand(OMX_CommandStateSet) // Enter OMX_StateExecuting.
 * 13.OMX_FillThisBuffer() // Drive OMX fill output buffers.
 * 14.OMX_EmptyThisBuffer() // Read frames from file and feed them into OMX.
 * 15.Wait for OMX_EventPortSettingsChanged to reconfigure output port per
 *    actual video info got by codec parsing video frame.
 * 16.alloc_nonsecure_buffer(out) // Allocate system memory to copy frame from
 *    secure output GBM buffer and dump it to a file for correctness checking.
 * 17.Wait for EOS event to free system resource and exit.
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

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

//#define ALOGE(fmt, args...) fprintf(stderr, fmt, ##args)
enum {
  PRIO_ERROR=0x1,
  PRIO_INFO=0x2,
  PRIO_HIGH=0x4,
  PRIO_LOW=0x8
};

#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#define getpid() syscall(SYS_getpid)

static int omx_debug_level = PRIO_ERROR;

void omx_debug_level_init(void)
{
  char *ptr = getenv("OMX_DEBUG_LEVEL");
  omx_debug_level = ptr ? atoi(ptr) : omx_debug_level;
  printf("omx_debug_level=0x%x\n", omx_debug_level);
}

#define DEBUG_PRINT_CTL(level, fmt, args...)   \
  do {                                        \
    if (level & omx_debug_level)           \
       printf("[%ld:%ld][%s:%d] " fmt "\n", getpid(), \
       gettid(), __func__, __LINE__, ##args); \
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
#include "secure_copy.h"

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

#define CONFIG_VERSION_SIZE(param) \
    param.nVersion.nVersion = CURRENT_OMX_SPEC_VERSION;\
    param.nSize = sizeof(param);

#define FAILED(result) (result != OMX_ErrorNone)

#define SUCCEEDED(result) (result == OMX_ErrorNone)
#define SWAPBYTES(ptrA, ptrB) { char t = *ptrA; *ptrA = *ptrB; *ptrB = t;}
#define SIZE_NAL_FIELD_MAX  4

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

/* flag indicates it's secure playback mode or not */
static bool secure_mode = false;
static uint8_t *input_nonsecure_buffer = NULL;
static uint8_t *output_nonsecure_buffer = NULL;

static int (*Read_Buffer)(uint8_t *data);

int inputBufferFileFd;

FILE * outputBufferFile;
int takeYuvLog = 0;
int num_frames_to_decode = 0;

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

unsigned int color_fmt_type = 0;

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
int nalSize = 0;
int sent_disabled = 0;
int waitForPortSettingsChanged = 1;
test_status currentStatus = GOOD_STATE;
struct timeval t_start = {0, 0}, t_end = {0, 0};
struct timeval t_main = {0, 0};

static bool kpi_mode = false;

//* OMX Spec Version supported by the wrappers. Version = 1.1 */
const OMX_U32 CURRENT_OMX_SPEC_VERSION = 0x00000101;
OMX_COMPONENTTYPE* dec_handle = 0;

OMX_BUFFERHEADERTYPE  **pInputBufHdrs = NULL;
OMX_BUFFERHEADERTYPE  **pOutYUVBufHdrs= NULL;

OMX_CONFIG_RECTTYPE crop_rect = {0,0,0,0};

/************************************************************************/
/*              GLOBAL FUNC DECL                        */
/************************************************************************/
int Init_Decoder(bool secure);
int Play_Decoder(bool secure);
int run_tests(bool secure);

/**************************************************************************/
/*              STATIC DECLARATIONS                       */
/**************************************************************************/
static int video_playback_count = 1;
static int open_video_file ();
static int Read_Buffer_From_H264_Start_Code_File(uint8_t *data);
static int Read_Buffer_From_H265_Start_Code_File(uint8_t *data);
static int Read_Buffer_From_Size_Nal(uint8_t *data);

static int fill_omx_input_buffer(OMX_BUFFERHEADERTYPE *omx_buf, bool secure);

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
//#define KPI_MARKER_NODE "/sys/kernel/debug/bootkpi/kpi_values"
#define KPI_MARKER_NODE "/sys/kernel/boot_kpi/kpi_values"	//TO DO: probably, it only fit for AGL/LXC-host case.
  int fd = open(KPI_MARKER_NODE, O_WRONLY);
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
    readBytes = fill_omx_input_buffer(pBuffer, secure_mode);
    if ((readBytes > 0) && !signal_eos) {
      pBuffer->nFilledLen = readBytes;
      DEBUG_PRINT("%s: Timestamp sent(%lld)", __FUNCTION__, pBuffer->nTimeStamp);
      OMX_EmptyThisBuffer(dec_handle,pBuffer);
      etb_count++;
    } else {
      pBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
      bInputEosReached = true;
      pBuffer->nFilledLen = 0;
      DEBUG_PRINT("%s: Timestamp sent(%lld)", __FUNCTION__, pBuffer->nTimeStamp);
      OMX_EmptyThisBuffer(dec_handle,pBuffer);
      DEBUG_PRINT("EBD::Either EOS or Some Error while reading file");
      etb_count++;
      break;
    }
  }
  return NULL;
}

static bool dump_yuv_frame_to_file(const uint8_t *data, size_t size)
{
  DEBUG_PRINT("format: 0x%x width: %d height: %d size: %u", color_fmt, crop_rect.nWidth, crop_rect.nHeight, size);

  if (NULL == data)
    return false;

  size_t n_written = 0;

  if (color_fmt == (OMX_COLOR_FORMATTYPE)QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m) {
    unsigned int stride = VENUS_Y_STRIDE(COLOR_FMT_NV12, portFmt.format.video.nFrameWidth);
    unsigned int scanlines = VENUS_Y_SCANLINES(COLOR_FMT_NV12, portFmt.format.video.nFrameHeight);
    const uint8_t *temp = data;

    temp += (stride * (int)crop_rect.nTop) +  (int)crop_rect.nLeft;
    for (int i = 0; i < crop_rect.nHeight; i++) {
      n_written = fwrite(temp, crop_rect.nWidth, 1, outputBufferFile);
      temp += stride;
    }

    temp = data + (stride * scanlines);
    temp += (stride * (int)crop_rect.nTop) +  (int)crop_rect.nLeft;
    for(int i = 0; i < crop_rect.nHeight/2; i++) {
      n_written += fwrite(temp, crop_rect.nWidth, 1, outputBufferFile);
      temp += stride;
    }
  } else {
    n_written = fwrite(data, size, 1, outputBufferFile);
  }

  if (n_written) {
    DEBUG_PRINT("FillBufferDone: Wrote %d YUV lines to the file", n_written);
    return true;
  } else {
    DEBUG_PRINT_ERROR("FillBufferDone: Failed to write to the file");
    return false;
  }
}

static void log_yuv_frame(const OMX_BUFFERHEADERTYPE *pBuffer, bool secure)
{
  uint8_t *yuv_data;
  bool ret = true;
  size_t size = pBuffer->nFilledLen;

  if (secure) {
    int buf_fd = (int)(long)pBuffer->pBuffer;
    yuv_data = output_nonsecure_buffer;
    DEBUG_PRINT("buf_fd=%d, nFilledLen=%u, yuv_data=0x%x", buf_fd, size, (long)yuv_data);
    bool ret = copy_from_secure_buffer(yuv_data, &size, buf_fd);
    if (ret) {
      static int copy_okay = 0;
      DEBUG_PRINT("copy_okay=%d", ++copy_okay);
    } else {
      static int copy_fail = 0;
      DEBUG_PRINT_ERROR("copy_fail=%d", ++copy_fail);
    }
  } else {
    yuv_data = (uint8_t *)pBuffer->pBuffer;
    DEBUG_PRINT("nFilledLen=%u, yuv_data=0x%x", size, (long)yuv_data);
  }

  if (ret)
    dump_yuv_frame_to_file(yuv_data, size);
}

void* fbd_thread(void* pArg)
{
  long unsigned act_time = 0, display_time = 0, render_time = 5e3, lipsync = 15e3;
  struct timeval t_avsync = {0, 0}, base_avsync = {0, 0};
  float total_time = 0;
  int contigous_drop_frame = 0, ret = 0;
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
        /* The pBuffer has pushed to fbd queuue, shouldn't be
         * used in the following context. Because the buffer maybe
         * old buffer before port reconfigures.
         */
        pBuffer = NULL;
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
        if (kpi_mode) {
          kpi_place_marker("M - Video Decoding 1st frame decoded");
        }
        gettimeofday(&t_start, NULL);
        int time_1st_cost_us = (t_start.tv_sec - t_main.tv_sec) * 1000000 + (t_start.tv_usec - t_main.tv_usec);
        printf("====>The first decoder output frame costs %d.%06d sec.\n",time_1st_cost_us/1000000,time_1st_cost_us%1000000);
      }
      fbd_cnt++;
      DEBUG_PRINT_ERROR("fbd_cnt=%d pBuffer=%p Timestamp=%lld", fbd_cnt, pBuffer, pBuffer->nTimeStamp);

      if (takeYuvLog)
        log_yuv_frame(pBuffer, secure_mode);
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

#define KPI_INDICATOR_STR "+kpi+"
#define SECURE_INDICATOR_STR "+secure+"

enum {
  OMX_LITE_NORMAL = 0,
  OMX_LITE_KPI = 0x1,
  OMX_LITE_SECURE = 0x2,
};

struct mode_desc {
  const char *str;
  int   len;
  int  mode;
};

static struct mode_desc modes[] = {
  { KPI_INDICATOR_STR, sizeof(KPI_INDICATOR_STR) - 1, OMX_LITE_KPI },
  { SECURE_INDICATOR_STR, sizeof(SECURE_INDICATOR_STR) - 1, OMX_LITE_SECURE },
};

#define NUM_MODES ARRAY_SIZE(modes)

int parse_argv1_mode_and_infile(const char *argv1)
{
  const char *filename;
  int i;

  printf("*** ");
  for (i = 0; i < NUM_MODES; i++) {
    if (!strncmp(argv1, modes[i].str, modes[i].len)) {
      filename = argv1 + modes[i].len;
      if (modes[i].mode & OMX_LITE_KPI) {
        kpi_mode = true;
        printf("Run in KPI mode\n");
        break;
      }
      if (modes[i].mode & OMX_LITE_SECURE) {
        secure_mode = true;
        printf("Run in secure mode\n");
        break;
      }
    }
  }

  if (NUM_MODES == i) {
    filename = argv1;
    printf("Run in normal mode\n");
  }

  if (strlen(filename) == 0) {
    printf("Input filename length is zero!\n");
    return -1;
  }

  if (access(filename, R_OK) < 0) {
    perror(filename);
    return -1;
  }
  strlcpy(in_filename, filename, sizeof(in_filename));

  if (kpi_mode) {
    // For early kpi mode, wait to ensure all is ready during board bootup.
    usleep(30000);
  }

  return 0;
}

static void parse_argv4_output_option(const char *argv4)
{
  int output_option = atoi(argv4);
  int format = QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m;

  takeYuvLog = 0;

  switch (output_option) {
  case 0: break;
  case 2: takeYuvLog = 1; break;
  case 8:
    format = QOMX_COLOR_FORMATYUV420PackedSemiPlanar32mCompressed;
    break;
  case 10:
    takeYuvLog = 1;
    format = QOMX_COLOR_FORMATYUV420PackedSemiPlanar32mCompressed;
    break;
  default:
    DEBUG_PRINT_ERROR("Output option %d unknown", output_option);
    break;
  }

  color_fmt = (OMX_COLOR_FORMATTYPE)format;
  DEBUG_PRINT("takeYuvLog=%d, color_fmt=0x%x", takeYuvLog, color_fmt);
}

static void print_usage(char **argv)
{
  printf("%s <infile_path> <codec_type> <file_type> <output_op> <test_op> <num_frames>\n", argv[0]);
  printf("<codec_type>\t1:h264, 9:h265\n");
  printf("<file_type>\t4:byte-stream\n");
  printf("<output_op>\t"
    "0: decoded as linear yuv but no output, 2: decoded as linear yuv but dump frames to yuvframes.yuv file under current dir\n\t\t"
    "8: decoded as UBWC yuv but no output,  10: decoded as UBWC yuv but dump frames to yuvframes.yuv file under current dir\n");
  printf("<test_op>\t1:decode till file end, 0:only decode <num_frame> frames\n");
  printf("<num_frames>\t0:when test_op=1, N:num of frames to decode when test_op=0\n\n");

  printf("Usage example(only verified h264 and h265):\n");
  printf("For h264: %s xxx.h264 1 4 2 1 0\n", argv[0]);
  printf("For h265: %s xxx.h265 9 4 2 1 0\n", argv[0]);
  printf("Above cmd will output NV12 file as yuvframes.yuv under current directory.\n\n");

  printf("For kpi mode, add %s before input file without blank\n", KPI_INDICATOR_STR);
  printf("Example: %s %s/data/xxx.h264 1 4 0 1 0\n\n", argv[0], KPI_INDICATOR_STR);

  printf("For secure mode, add %s before input file without blank\n", SECURE_INDICATOR_STR);
  printf("Example: %s %s/data/xxx.h264 1 4 0 1 0\n\n", argv[0], SECURE_INDICATOR_STR);
}

int main(int argc, char **argv)
{
  int test_option = 0;
  int pic_order = 0;
  OMX_ERRORTYPE result;
  sliceheight = height = 144;
  stride = width = 176;

  crop_rect.nLeft = 0;
  crop_rect.nTop = 0;
  crop_rect.nWidth = width;
  crop_rect.nHeight = height;

  omx_debug_level_init();

  if (argc < 7)
  {
    print_usage(argv);
    return -1;
  }
  else if(argc >= 7)
  {
    codec_format_option = (codec_format)atoi(argv[2]);
    file_type_option = (file_type)atoi(argv[3]);
    parse_argv4_output_option(argv[4]);
    test_option = atoi(argv[5]);
    if (0 == test_option) {
      num_frames_to_decode = atoi(argv[6]);
    } else if (1 == test_option) {
      /* play the clip till the end */
      num_frames_to_decode = 0;
    }
    printf("*******************************************************\n");
    printf("*** codec=%d,file_type=%d,output=%d,test=%d,frames=%d\n",
        codec_format_option, file_type_option,
        atoi(argv[4]), test_option, num_frames_to_decode);
  }

  if (parse_argv1_mode_and_infile(argv[1]))
    return -1;

  if (file_type_option >= FILE_TYPE_COMMON_CODEC_MAX)
  {
    switch (codec_format_option)
    {
      case CODEC_FORMAT_H264:
        file_type_option = (file_type)(FILE_TYPE_START_OF_H264_SPECIFIC + file_type_option - FILE_TYPE_COMMON_CODEC_MAX);
        break;
      case CODEC_FORMAT_HEVC:
        file_type_option = (file_type)(FILE_TYPE_START_OF_H265_SPECIFIC + file_type_option - FILE_TYPE_COMMON_CODEC_MAX);
        break;
      default:
        DEBUG_PRINT_ERROR("Error: Unknown code %d", codec_format_option);
        return -1;
    }
  }

  CONFIG_VERSION_SIZE(picture_order);
  picture_order.eOutputPictureOrder = QOMX_VIDEO_DISPLAY_ORDER;
  if (pic_order == 1)
    picture_order.eOutputPictureOrder = QOMX_VIDEO_DECODE_ORDER;

  printf("Input values: inputfilename[%s]\n", in_filename);
  printf("*******************************************************\n");
  gettimeofday(&t_main, NULL);
  if (kpi_mode) {
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
  pthread_setname_np(fbd_thread_id, "fbd_thread");

  run_tests(secure_mode);

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

int run_tests(bool secure)
{
  int cmd_error = 0;
  DEBUG_PRINT("Inside %s", __FUNCTION__);
  waitForPortSettingsChanged = 1;

  if(codec_format_option == CODEC_FORMAT_H264) {
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

  DEBUG_PRINT("file_type_option %d!", file_type_option);

  switch(file_type_option)
  {
    case FILE_TYPE_264_START_CODE_BASED:
    case FILE_TYPE_264_NAL_SIZE_LENGTH:
    case FILE_TYPE_265_START_CODE_BASED:
      if(Init_Decoder(secure) != 0x00)
      {
        DEBUG_PRINT_ERROR("Error - Decoder Init failed");
        return -1;
      }
      if(Play_Decoder(secure) != 0x00)
      {
        return -1;
      }
      break;
    default:
      DEBUG_PRINT_ERROR("Error - Invalid Entry...%d",file_type_option);
      return -1;
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

static bool alloc_nonsecure_buffer(uint8_t **buf, size_t size)
{
  *buf = (uint8_t *)malloc(size);
  if (NULL == *buf) {
    perror(__func__);
    return false;
  }

  return true;
}

static void free_nonsecure_buffer(uint8_t **buf)
{
  if (*buf) {
    free(*buf);
    *buf = NULL;
  }
}

static int fill_omx_input_buffer(OMX_BUFFERHEADERTYPE *omx_buf, bool secure)
{
  uint8_t *data = NULL;

  if (secure) {
    data = input_nonsecure_buffer;
  } else {
    data = omx_buf->pBuffer;
  }

  ssize_t bytes = Read_Buffer(data);
  if (bytes <= 0)
    return bytes;

  omx_buf->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;

  if (secure) {
    int sec_buf_fd = (int)(long)(omx_buf->pBuffer);
    int ret = copy_to_secure_buffer(data, (size_t *)&bytes, sec_buf_fd);
    if (!ret)
      return -1;
  }

  return (int)bytes;
}

static bool get_omx_component_name(char *name, size_t size, bool secure)
{
  const char *cname;

  switch (codec_format_option) {
  case CODEC_FORMAT_H264:
    cname = "OMX.qcom.video.decoder.avc";
    break;
  case CODEC_FORMAT_HEVC:
    cname = "OMX.qcom.video.decoder.hevc";
    break;
  default:
    DEBUG_PRINT_ERROR("Unsupported codec %d", codec_format_option);
    return false;
  }

  if (strlcpy(name, cname, size) >= size)
    return false;

  if (secure)
    if (strlcat(name, ".secure", size) >= size)
      return false;

  return true;
}

int Init_Decoder(bool secure)
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

  if (!get_omx_component_name(vdecCompNames, sizeof(vdecCompNames), secure))
    return -1;

  omxresult = OMX_GetHandle((OMX_HANDLETYPE*)(&dec_handle),
                    (OMX_STRING)vdecCompNames, NULL, &call_back);
  if (FAILED(omxresult)) {
    DEBUG_PRINT_ERROR("Failed to Load the component:%s", vdecCompNames);
    printf("Failed to Load the component:%s, OMX_GetHandle() ret 0x%08x\n", vdecCompNames, omxresult);
    if (kpi_mode) {
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
  else if (codec_format_option == CODEC_FORMAT_HEVC)
  {
    portFmt.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)QOMX_VIDEO_CodingHevc;
  }
  else
  {
    DEBUG_PRINT_ERROR("Error: Unsupported codec %d", codec_format_option);
    return -1;
  }

  return 0;
}

int Play_Decoder(bool secure)
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
    case FILE_TYPE_264_START_CODE_BASED:
    case FILE_TYPE_265_START_CODE_BASED:
    {
      inputPortFmt.nFramePackingFormat = OMX_QCOM_FramePacking_OnlyOneCompleteFrame;
      break;
    }
    case FILE_TYPE_264_NAL_SIZE_LENGTH:
    {
      inputPortFmt.nFramePackingFormat = OMX_QCOM_FramePacking_Arbitrary;
      break;
    }
    default:
      inputPortFmt.nFramePackingFormat = OMX_QCOM_FramePacking_Unspecified;
  }
  OMX_SetParameter(dec_handle,(OMX_INDEXTYPE)OMX_QcomIndexPortDefn,
             (OMX_PTR)&inputPortFmt);

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

  /* enumerate to set color format */
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

  if (secure)
    if (!alloc_nonsecure_buffer(&input_nonsecure_buffer, portFmt.nBufferSize))
      return -1;

  /* Below are output port initialization. */
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
  for (i = 0; i < used_ip_buf_cnt; i++) {
    pInputBufHdrs[i]->nInputPortIndex = 0;
    pInputBufHdrs[i]->nOffset = 0;
    if ((frameSize = fill_omx_input_buffer(pInputBufHdrs[i], secure)) <= 0) {
      DEBUG_PRINT("NO FRAME READ");
      pInputBufHdrs[i]->nFilledLen = 0;
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
  pthread_setname_np(ebd_thread_id, "ebd_thread");

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

  if (secure && takeYuvLog)
    if (!alloc_nonsecure_buffer(&output_nonsecure_buffer, portFmt.nBufferSize))
      return -1;

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
    if (pOutYUVBufHdrs) {
      free(pOutYUVBufHdrs);
      pOutYUVBufHdrs = NULL;
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

  free_nonsecure_buffer(&input_nonsecure_buffer);
  free_nonsecure_buffer(&output_nonsecure_buffer);

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

static int Read_Buffer_From_H264_Start_Code_File(uint8_t *data)
{
  int bytes_read = 0;
  int cnt = 0;
  unsigned int code = 0;
  int naluType = 0;
  int newFrame = 0;
  char *dataptr = (char *)data;
  DEBUG_PRINT("Inside %s", __FUNCTION__);
  do
  {
    newFrame = 0;
    bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
    if (!bytes_read)
    {
      DEBUG_PRINT("Bytes read Zero, cnt=%d", cnt);
      break;
    }
    code <<= 8;
    code |= (0x000000FF & dataptr[cnt]);
    cnt++;
    if ((cnt == 4) && (code != H264_START_CODE))
    {
      DEBUG_PRINT_ERROR("ERROR: Invalid start code found 0x%x", code);
      cnt = 0;
      break;
    }
    if ((cnt > 4) && (code == H264_START_CODE))
    {
      DEBUG_PRINT("Found H264_START_CODE");
      bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
      if (!bytes_read)
      {
        DEBUG_PRINT("Bytes read Zero");
        break;
      }
      DEBUG_PRINT("READ Byte[%d] = 0x%x", cnt, dataptr[cnt]);
      naluType = dataptr[cnt] & 0x1F;
      cnt++;
      if ((naluType == 1) || (naluType == 5))
      {
        DEBUG_PRINT("Found AU");
        bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
        if (!bytes_read)
        {
          DEBUG_PRINT("Bytes read Zero");
          break;
        }
        DEBUG_PRINT("READ Byte[%d] = 0x%x", cnt, dataptr[cnt]);
        newFrame = (dataptr[cnt] & 0x80);
        cnt++;
        if (newFrame)
        {
          lseek64(inputBufferFileFd, -6, SEEK_CUR);
          cnt -= 6;
          DEBUG_PRINT("Found a NAL unit (type 0x%x) of size = %d", (dataptr[4] & 0x1F), cnt);
          break;
        }
        else
        {
          DEBUG_PRINT("Not a New Frame");
        }
      }
      else
      {
        lseek64(inputBufferFileFd, -5, SEEK_CUR);
        cnt -= 5;
        DEBUG_PRINT("Found NAL unit (type 0x%x) of size = %d", (dataptr[4] & 0x1F), cnt);
        break;
      }
    }
  } while (1);

  return cnt;
}
static int Read_Buffer_From_H265_Start_Code_File(uint8_t *data)
{
  int bytes_read = 0;
  int cnt = 0;
  unsigned int code = 0;
  int naluType = 0;
  int newFrame = 0;
  char *dataptr = (char *)data;
  DEBUG_PRINT("Inside %s", __FUNCTION__);
  do
  {
    newFrame = 0;
    bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
    if (!bytes_read)
    {
      DEBUG_PRINT("Bytes read Zero, cnt=%d", cnt);
      break;
    }
    code <<= 8;
    code |= (0x000000FF & dataptr[cnt]);
    cnt++;
    if ((cnt == 4) && (code != H265_START_CODE))
    {
      DEBUG_PRINT_ERROR("ERROR: Invalid start code found 0x%x", code);
      cnt = 0;
      break;
    }
    if ((cnt > 4) && (code == H265_START_CODE))
    {
      DEBUG_PRINT("Found H265_START_CODE");
      bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
      if (!bytes_read)
      {
        DEBUG_PRINT("Bytes read Zero");
        break;
      }
      DEBUG_PRINT("READ Byte[%d] = 0x%x", cnt, dataptr[cnt]);
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
        DEBUG_PRINT("Found AU for HEVC");
        bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
        if (!bytes_read)
        {
          DEBUG_PRINT("Bytes read Zero");
          break;
        }
        cnt ++;
        bytes_read = read(inputBufferFileFd, &dataptr[cnt], 1);
        DEBUG_PRINT("READ Byte[%d] = 0x%x firstsliceflag %d", cnt, dataptr[cnt], dataptr[cnt] >> 7);
        newFrame = (dataptr[cnt] >>7); //firstsliceflag
        cnt++;
        if (newFrame)
        {
          lseek64(inputBufferFileFd, -7, SEEK_CUR);
          cnt -= 7;
          DEBUG_PRINT("Found a NAL unit (type 0x%x) of size = %d", ((dataptr[4] & HEVC_NAL_UNIT_TYPE_MASK) >> 1), cnt);
          break;
        }
        else
        {
          DEBUG_PRINT("Not a New Frame");
        }
      }
      else
      {
        lseek64(inputBufferFileFd, -5, SEEK_CUR);
        cnt -= 5;
        DEBUG_PRINT("Found NAL unit (type 0x%x) of size = %d", ((dataptr[4] & HEVC_NAL_UNIT_TYPE_MASK) >> 1), cnt);
        break;
      }
    }
  } while (1);

  return cnt;
}

static int Read_Buffer_From_Size_Nal(uint8_t *data)
{
  // NAL unit stream processing
  char temp_size[SIZE_NAL_FIELD_MAX];
  int i = 0;
  int j = 0;
  unsigned int size = 0;   // Need to make sure that uint32 has SIZE_NAL_FIELD_MAX (4) bytes
  int bytes_read = 0;

  // read the "size_nal_field"-byte size field
  bytes_read = read(inputBufferFileFd, data, nalSize);
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
    temp_size[SIZE_NAL_FIELD_MAX - 1 - i] = ((char *)data)[j];
  }
  size = (unsigned int)(*((unsigned int *)(temp_size)));

  // now read the data
  bytes_read = read(inputBufferFileFd, data + nalSize, size);
  if (bytes_read != size)
  {
    DEBUG_PRINT_ERROR("Failed to read frame");
  }

  return bytes_read + nalSize;
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
