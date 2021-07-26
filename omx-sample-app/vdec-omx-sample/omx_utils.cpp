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
#include <errno.h>
//  OMX
#include "OMX_QCOMExtns.h"
#include "OMX_Core.h"
#include "OMX_Video.h"
#include "OMX_VideoExt.h"
#include "OMX_IndexExt.h"
#include "OMX_Component.h"
#include "OMX_Types.h"

#include "video_test_debug.h"

#include "video_queue.h"
#include "omx_utils.h"
#include <vidc/media/msm_media_info.h>

#define FractionToQ16(q, v) { (q) = (unsigned int) (65536*(double)(v)); }

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

#define OMX_INIT_STRUCT_SIZE(_s_, _name_)            \
  (_s_)->nSize = sizeof(_name_);               \
(_s_)->nVersion.nVersion = OMX_SPEC_VERSION

#define DEFAULT_TILE_DIMENSION 512
#define DEFAULT_TILE_COUNT 40
#define DEFAULT_WIDTH_ALIGNMENT 128
#define DEFAULT_HEIGHT_ALIGNMENT 32

// FPS control mode
extern int32_t m_FpsControl;

static int (*ReadBuffer)(OMX_BUFFERHEADERTYPE *pBufferHeader);
static int m_InputFileFd = -1;
static int m_OutputFileFd = -1;

static OMX_STATETYPE m_eState = OMX_StateLoaded;
static OMX_STATETYPE m_PendingEstate = OMX_StateLoaded;
static OMX_HANDLETYPE m_Handle = NULL;

static int32_t m_InputBufferSize = 0;
static int32_t m_InputBufferCount = 0;
static int32_t m_OutputBufferSize = 0;
static int32_t m_OutputBufferCount = 0;
static int32_t m_FreedOutputBufferCnt = 0;

static const int OMX_BUFFERS_NUM = 20;
OMX_BUFFERHEADERTYPE* m_InputBufferHeaders[OMX_BUFFERS_NUM] = {NULL};
OMX_BUFFERHEADERTYPE* m_OutputBufferHeaders[OMX_BUFFERS_NUM] = {NULL};

static pthread_t m_OutPutProcessThreadId;
static bool m_OutputThreadRunning = false;
static pthread_t m_InputProcessThreadId;
static bool m_InputThreadRunning = false;
static uint32_t m_FillFramNum = 0;

// preference debug
int64_t m_DecodeTotalTimeActal = 0;
int64_t m_DecodeFrameTimeMax = 0;
int64_t m_DecodeFrameTimeMin = 0;

int32_t m_InputFrameNum = 0;
int32_t m_OutputFrameNum = 0;
int32_t m_InputDataSize = 0;
int32_t m_OutputDataSize = 0;

struct timeval m_DecodeStartTime;
struct timeval m_DecodeEndTime;

#define MAX_WAIT_TIMES 5
#define MAX_WAIT_RECONFIG_DURATION 2 //seconds

bool m_IsSWCodec = false;
int waitForFirstTimeReconfigEvent = 0;
uint32_t m_ConfigureColorFormat = 0;
volatile int m_EventComplete = 0;
static bool m_NeedReconfigPort = false;
static pthread_mutex_t m_ReconfigMutex;
static pthread_mutex_t m_EventCompleteMutex;
static pthread_cond_t  m_ReconfigSignal;
static pthread_cond_t  m_EventCompleteSignal;
// used to wait codec component stop notify
static bool m_Running = false;
static pthread_mutex_t m_RunningMutex;
static pthread_cond_t  m_RunningSignal;

struct dec_ion* mIonDataArray = NULL;

// need to confirm wether need to set by config
static OMX_S64 timeStampLfile = 0;
unsigned int timestampInterval = 33333;

/* Bit 6 of the type indicates V1 if 0, V2 if 1 */
#define RCV_VC1_TYPE (0x85)
/* Top nibble bits of frame size word are flags in V2 */
#define RCV_V2_MASK (1 << 6)
#define RCV_V2_FRAMESIZE_FLAGS (0xf0000000)
#define RCV_V2_KEYFRAME_FLAG   (0x80000000)
static bool bRCVFirstRead = true;
static bool m_bRCV_V2 = false;
static int previous_vc1_au = 0;
static int bHdrflag = 0;
static int rcv_v1 = 0;
int nalSize = 0;
OMX_CONFIG_RECTTYPE crop_rect = {0, 0, 0, 0};
static VideoCodecSetting_t *settings;

int64_t m_DecodeDuration = 0; // in usec
timeval m_DecodeFrameStartTime[OMX_BUFFERS_NUM] = {};  // used to compute decode frame performance
timeval m_DecodeFrameEndTime[OMX_BUFFERS_NUM] = {};  // used to compute decode frame performance

// For Statistical data
static int32_t m_decodeFrameNum = 0;
static float   m_totalTime = 0.0;
static int32_t m_inputDataSize = 0;
static int32_t m_outputDataSize = 0;
static int m_nInputFrameCnt = 0;
static int m_nOutputFrameCnt = 0;
struct timeval m_decodeStartTime;
struct timeval m_decodeEndTime;

static int OpenVideoFile ()
{
  int error_code = 0;
  char outputfilename[512];
  FUNCTION_ENTER();
  VLOGD("filename is %s", m_InputFileName);

  if ((m_InputFileFd = open(m_InputFileName, O_RDONLY | O_LARGEFILE)) == -1)
  {
    VLOGE("Error: i/p file %s could NOT be opened errno = %d", m_InputFileName, errno);
    error_code = -1;
  }
  else
  {
    VLOGD("i/p file %s is opened ", m_InputFileName);
  }

  FUNCTION_EXIT();
  return error_code;
}


static int ReadBufferFromDATFile(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  long frame_size=0;
  char temp_buffer[10];
  char temp_byte;
  int bytes_read=0;
  int i=0;
  //unsigned char *read_buffer=NULL;
  char c = '1'; //initialize to anything except '\0'(0)
  char input_frame_size[12];
  int count =0; char cnt =0;
  memset(temp_buffer, 0, sizeof(temp_buffer));

  FUNCTION_ENTER();

  //Check the input file format, may result in infinite loop
  while (cnt < 10)
  {
    VLOGD("loop[%d] count[%d]",cnt,count);
    count = read(m_InputFileFd, &input_frame_size[cnt], 1);
    if(input_frame_size[cnt] == '\0')
      break;
    cnt++;
  }
  input_frame_size[cnt]='\0';
  frame_size = atoi(input_frame_size);
  pBufHdr->nFilledLen = 0;

  /* get the frame length */
  lseek64(m_InputFileFd, -1, SEEK_CUR);
  bytes_read = read(m_InputFileFd, pBufHdr->pBuffer, frame_size);

  VLOGD("Actual frame Size [%d] bytes_read using fread[%d]",
      frame_size, bytes_read);

  if(bytes_read == 0 || bytes_read < frame_size)
  {
    VLOGE("Bytes read Zero After Read frame Size");
    FUNCTION_EXIT();
    return 0;
  }
  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;
  FUNCTION_EXIT();
  return bytes_read;
}

static int ReadBufferFromH264StartCodeFile(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  int bytes_read = 0;
  int cnt = 0;
  unsigned int code = 0;
  int nalu_type = 0;
  int new_frame = 0;
  char *dataptr = (char *)pBufHdr->pBuffer;
  FUNCTION_ENTER();
  do
  {
    new_frame = 0;
    bytes_read = read(m_InputFileFd, &dataptr[cnt], 1);
    if (bytes_read <= 0)
    {
      VLOGD("Bytes read Zero");
      break;
    }
    code <<= 8;
    code |= (0x000000FF & dataptr[cnt]);
    cnt++;
    if ((cnt == 4) && (code != H264_START_CODE))
    {
      VLOGE("ERROR: Invalid start code found 0x%x", code);
      cnt = 0;
      break;
    }
    if ((cnt > 4) && (code == H264_START_CODE))
    {
      VLOGD("Found H264_START_CODE");
      bytes_read = read(m_InputFileFd, &dataptr[cnt], 1);
      if (!bytes_read)
      {
        VLOGD("Bytes read Zero");
        break;
      }
      VLOGD("READ Byte[%d] = 0x%x", cnt, dataptr[cnt]);
      nalu_type = dataptr[cnt] & 0x1F;
      cnt++;
      if ((nalu_type == 1) || (nalu_type == 5))
      {
        VLOGD("Found AU");
        bytes_read = read(m_InputFileFd, &dataptr[cnt], 1);
        if (!bytes_read)
        {
          VLOGD("Bytes read Zero");
          break;
        }
        VLOGD("READ Byte[%d] = 0x%x", cnt, dataptr[cnt]);
        new_frame = (dataptr[cnt] & 0x80);
        cnt++;
        if (new_frame)
        {
          lseek64(m_InputFileFd, -6, SEEK_CUR);
          cnt -= 6;
          VLOGD("Found a NAL unit (type 0x%x) of size = %d", (dataptr[4] & 0x1F), cnt);
          break;
        }
        else
        {
          VLOGD("Not a New Frame");
        }
      }
      else
      {
        lseek64(m_InputFileFd, -5, SEEK_CUR);
        cnt -= 5;
        VLOGD("Found NAL unit (type 0x%x) of size = %d", (dataptr[4] & 0x1F), cnt);
        break;
      }
    }
  } while (1);

  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;
  FUNCTION_EXIT();
  return cnt;
}

static int ReadBufferArbitraryBytes(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  int bytes_read=0;
  FUNCTION_ENTER();
  bytes_read = read(m_InputFileFd, pBufHdr->pBuffer, NUMBER_OF_ARBITRARYBYTES_READ);
  if(bytes_read == 0) {
    VLOGD("Bytes read Zero After Read frame Size ");
    FUNCTION_EXIT();
    return 0;
  }
  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;
  FUNCTION_EXIT();
  return bytes_read;
}

static int ReadBufferFromVopStartCodeFile(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  unsigned int readOffset = 0;
  int bytes_read = 0;
  unsigned int code = 0;
  pBufHdr->nFilledLen = 0;
  static unsigned int header_code = 0;

  FUNCTION_ENTER();

  do
  {
    //Start codes are always byte aligned.
    bytes_read = read(m_InputFileFd, &pBufHdr->pBuffer[readOffset], 1);
    if(bytes_read == 0 || bytes_read == -1)
    {
      VLOGD("Bytes read Zero ");
      break;
    }
    code <<= 8;
    code |= (0x000000FF & pBufHdr->pBuffer[readOffset]);
    //VOP start code comparision
    if (readOffset > 3)
    {
      if(!header_code)
      {
        if(VOP_START_CODE == code)
        {
          header_code = VOP_START_CODE;
        }
        else if ((0xFFFFFC00 & code) == SHORT_HEADER_START_CODE)
        {
          header_code = SHORT_HEADER_START_CODE;
        }
      }
      if ((header_code == VOP_START_CODE) && (code == VOP_START_CODE))
      {
        //Seek backwards by 4
        lseek64(m_InputFileFd, -4, SEEK_CUR);
        readOffset-=3;
        break;
      }
      else if ((header_code == SHORT_HEADER_START_CODE) &&
          (SHORT_HEADER_START_CODE == (code & 0xFFFFFC00)))
      {
        //Seek backwards by 4
        lseek64(m_InputFileFd, -4, SEEK_CUR);
        readOffset-=3;
        break;
      }
    }
    readOffset++;
  }while (1);
  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;
  FUNCTION_EXIT();
  return readOffset;
}

static int ReadBufferFromMpeg2StartCode(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  unsigned int readOffset = 0;
  int bytes_read = 0;
  unsigned int code = 0;
  pBufHdr->nFilledLen = 0;
  static unsigned int first_parse = true;
  unsigned int see_nframe = false;

  FUNCTION_ENTER();

  /* Read one byte at a time. Construct the code every byte in order to
   * compare to the start codes. Keep looping until we've read in a complete
   * frame, which can be either just a picture start code + picture, or can
   * include the sequence header as well
   */
  while (1)
  {
    bytes_read = read(m_InputFileFd, &pBufHdr->pBuffer[readOffset], 1);

    /* Exit the loop if we can't read any more bytes */
    if (bytes_read == 0 || bytes_read == -1)
    {
      break;
    }

    /* Construct the code one byte at a time */
    code <<= 8;
    code |= (0x000000FF & pBufHdr->pBuffer[readOffset]);

    /* Can't compare the code to MPEG2 start codes until we've read the
     * first four bytes
     */
    if (readOffset >= 3)
    {

      /* If this is the first time we're reading from the file, then we
       * need to throw away the system start code information at the
       * beginning. We can just look for the first sequence header.
       */
      if (first_parse)
      {
        if (code == MPEG2_SEQ_START_CODE)
        {
          /* Seek back by 4 bytes and reset code so that we can skip
           * down to the common case below.
           */
          lseek(m_InputFileFd, -4, SEEK_CUR);
          code = 0;
          readOffset -= 3;
          first_parse = false;
          continue;
        }
      }

      /* If we have already parsed a frame and we see a sequence header, then
       * the sequence header is part of the next frame so we seek back and
       * break.
       */
      if (code == MPEG2_SEQ_START_CODE)
      {
        if (see_nframe)
        {
          lseek(m_InputFileFd, -4, SEEK_CUR);
          readOffset -= 3;
          break;
        }
        /* If we haven't seen a frame yet, then read in all the data until we
         * either see another frame start code or sequence header start code.
         */
      }
      else if (code == MPEG2_FRAME_START_CODE)
      {
        if (!see_nframe)
        {
          see_nframe = true;
        }
        else
        {
          lseek(m_InputFileFd, -4, SEEK_CUR);
          readOffset -= 3;
          break;
        }
      }
    }

    readOffset++;
  }

  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;
  FUNCTION_EXIT();
  return readOffset;
}

static int ReadBufferFromSizeNal(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  // NAL unit stream processing
  char temp_size[SIZE_NAL_FIELD_MAX];
  int i = 0;
  int j = 0;
  unsigned int size = 0;   // Need to make sure that uint32 has SIZE_NAL_FIELD_MAX (4) bytes
  int bytes_read = 0;

  FUNCTION_ENTER();

  // read the "size_nal_field"-byte size field
  bytes_read = read(m_InputFileFd, pBufHdr->pBuffer + pBufHdr->nOffset, nalSize);
  if (bytes_read == 0 || bytes_read == -1)
  {
    VLOGD("Failed to read frame or it might be EOF");
    FUNCTION_EXIT();
    return 0;
  }

  for (i = 0; i < SIZE_NAL_FIELD_MAX - nalSize; i++)
  {
    temp_size[SIZE_NAL_FIELD_MAX - 1 - i] = 0;
  }

  /* Due to little endiannes, Reorder the size based on size_nal_field */
  for (j = 0; i < SIZE_NAL_FIELD_MAX; i++, j++)
  {
    temp_size[SIZE_NAL_FIELD_MAX - 1 - i] = pBufHdr->pBuffer[pBufHdr->nOffset + j];
  }
  size = (unsigned int)(*((unsigned int *)(temp_size)));

  // now read the data
  bytes_read = read(m_InputFileFd, pBufHdr->pBuffer + pBufHdr->nOffset + nalSize, size);
  if (bytes_read != size)
  {
    VLOGE("Failed to read frame");
  }

  pBufHdr->nTimeStamp = timeStampLfile;
  timeStampLfile += timestampInterval;

  FUNCTION_EXIT();
  return bytes_read + nalSize;
}

static int ReadBufferFromVC1RCVFile(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  // read() is not safe and coverable enough for abnormal case.
  // read function in VideoStreamParser can be referred.
  unsigned int readOffset = 0;
  int size_struct_C = 0;
  int frameType = 0;
  // Should be full seqlayer all the time
  bool bRCVFullSeqLayer = true;
  FUNCTION_ENTER();

  VLOGD("Read VC1RCV file: pBuffer %p", pBufHdr->pBuffer);
  OMX_U8 *pBuffer = pBufHdr->pBuffer + pBufHdr->nOffset;

  unsigned long long nCurFileOffset = lseek64(m_InputFileFd, 0, SEEK_CUR);
  VLOGD("RCV file current file offset is %lld", nCurFileOffset);

  if (bRCVFirstRead)
  {
    int nFrames, nHeight, nWidth;
    /* The first 3 bytes are the number of frames */
    if (read(m_InputFileFd, &nFrames, 3) <= 0)
    {
      VLOGE("read frame numbers failed with errno: %d", errno);
      FUNCTION_EXIT();
      return 0;
    }
    readOffset += 3;
    VLOGD("Numbers of Frame: %d", (nFrames & 0x00FFFFFF));

    /* The next byte is the type and extension flag */
    if (read(m_InputFileFd, &nFrames, 1) <= 0)
    {
      VLOGE("read key failed with errno: %d", errno);
      FUNCTION_EXIT();
      return 0;
    }
    VLOGD("Key: %d", (nFrames & 0xFF));

    readOffset += 1;
    m_bRCV_V2 = ((nFrames & 0xFF) & RCV_V2_MASK) ? true : false;

    /* Next 4 bytes are the size of the extension data */
    if (read(m_InputFileFd, &size_struct_C, 4) <= 0)
    {
      VLOGE("read struct C failed with errno: %d", errno);
      FUNCTION_EXIT();
      return 0;
    }
    VLOGD("size of struct C: %d", size_struct_C);
    readOffset += 4;

    /* parse sequence Header */
    lseek64(m_InputFileFd, size_struct_C, SEEK_CUR);
    readOffset += size_struct_C;
    /*Read nHeight 4 bytes*/
    if (read(m_InputFileFd, &nHeight, 4) <= 0)
    {
      VLOGE("read height failed with errno: %d", errno);
      FUNCTION_EXIT();
      return 0;
    }
    readOffset += 4;
    /*Read nWidth 4 bytes*/
    if (read(m_InputFileFd, &nWidth, 4) <= 0)
    {
      VLOGE("read width failed with errno: %d", errno);
      FUNCTION_EXIT();
      return 0;
    }
    readOffset += 4;
    VLOGD("Width: %d, Height: %d", nWidth, nHeight);

    if (m_bRCV_V2)
    {
      int add_data_size = 0;
      //for parsing rcv v2 seq header, fw doesn't need these other information
      if (read(m_InputFileFd, &add_data_size, 4) <= 0)
      {
        VLOGE("read addition data failed with errno: %d", errno);
        FUNCTION_EXIT();
        return 0;
      }
      readOffset += 4;
      lseek64(m_InputFileFd, add_data_size, SEEK_CUR); //Ignore additional data
      readOffset += add_data_size;
    }
    if (!bRCVFullSeqLayer)
    {
      readOffset = size_struct_C;
      nCurFileOffset += 8;
    }
    bRCVFirstRead = false;
    pBufHdr->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
    VLOGD("1ST frame, file offset: %d, read offset: %d", nCurFileOffset, readOffset);
  }
  else
  {
    if (read(m_InputFileFd, &size_struct_C, 4) <= 0)
    {
      VLOGE("read struct C failed with errno: %d", errno);
      FUNCTION_EXIT();
      return 0;
    }
    VLOGD("size_struct_C: %d", size_struct_C);
    frameType = size_struct_C & 0x80000000;
    readOffset += 4;

    if (m_bRCV_V2)
    {
      /* Mask off the flag bits */
      size_struct_C = size_struct_C & ~RCV_V2_FRAMESIZE_FLAGS;
      /* Skip the timestamp */
      lseek64(m_InputFileFd, 4, SEEK_CUR);
      readOffset += 4;
      nCurFileOffset += 8; // for ignoing size/timestamp
      readOffset -= 8;
    }
    else
    {
      nCurFileOffset += 4; // for ignoing size
      readOffset -= 4;
    }
    VLOGD("From 2ND frame, file offset: %d, read offset: %d", nCurFileOffset, readOffset);
    readOffset += size_struct_C;
    lseek64(m_InputFileFd, size_struct_C, SEEK_CUR);
  }
  // to restore frameoffset after frame data read
  unsigned long long nFrameOffset = lseek64(m_InputFileFd, 0, SEEK_CUR);
  lseek64(m_InputFileFd, nCurFileOffset, SEEK_SET);
  if (readOffset < pBufHdr->nAllocLen)
  {
    if (read(m_InputFileFd, pBufHdr->pBuffer, readOffset) <= 0)
    {
      VLOGE("read failed with errno: %d", errno);
      FUNCTION_EXIT();
      return 0;
    }
    VLOGD("FrameSize: %d read, curr file offset: %#X", readOffset, nFrameOffset);
    lseek64(m_InputFileFd, nFrameOffset, SEEK_SET);
  }
  else
  {
    VLOGE("FrameSize: %d read is More than AllocLength: %d", readOffset, pBufHdr->nAllocLen);
    readOffset = 0;
  }
  pBufHdr->nFilledLen = readOffset;

  FUNCTION_EXIT();
  return readOffset;
}

#ifdef __DEBUG_DIVX__
static int ReadBufferFromDivX456File(OMX_BUFFERHEADERTYPE  *pBufHdr)
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

  FUNCTION_ENTER();

  do {
    p_buffer = (char *)pBufHdr->pBuffer + byte_pos;

    bytes_read = read(m_InputFileFd, p_buffer, NUMBER_OF_ARBITRARYBYTES_READ);
    byte_pos += bytes_read;
    for (byte_cntr = 0; byte_cntr < bytes_read && !pckt_ready; byte_cntr++)
    {
      read_code <<= 8;
      ((char*)&read_code)[0] = p_buffer[byte_cntr];
      if (read_code == VOP_START_CODE)
      {
        if (++byte_cntr < bytes_read)
        {
          frame_type = p_buffer[byte_cntr];
          frame_type &= 0x000000C0;
#ifdef __DEBUG_DIVX__
          switch (frame_type)
          {
            case 0x00: pckt_type[pckd_frms] = 'I'; break;
            case 0x40: pckt_type[pckd_frms] = 'P'; break;
            case 0x80: pckt_type[pckd_frms] = 'B'; break;
            default: pckt_type[pckd_frms] = 'X';
          }
          pckd_frms++;
#endif // __DEBUG_DIVX__
          offset_array[vop_set_cntr] = byte_pos - bytes_read + byte_cntr - 4;
          if (frame_type == 0x80)
          {
            // B Frame found!
            if (!b_frames_found)
            {
              // Try to packet N_PREV_FRMS_B previous frames
              // with the next consecutive B frames
              i = N_PREV_FRMS_B;
              while ((vop_set_cntr - i) < 0 && i > 0) i--;
              b_frm_idx = vop_set_cntr - i;
              if (b_frm_idx > 0)
              {
                pckt_end_idx = b_frm_idx;
                pckt_ready = true;
#ifdef __DEBUG_DIVX__
                pckt_type[b_frm_idx] = '\0';
                total_frames += b_frm_idx;
#endif //__DEBUG_DIVX__
              }
            }
            b_frames_found++;
          }
          else if (b_frames_found)
          {
            pckt_end_idx = vop_set_cntr;
            pckt_ready = true;
#ifdef __DEBUG_DIVX__
            pckt_type[pckd_frms - 1] = '\0';
            total_frames += pckd_frms - 1;
#endif //__DEBUG_DIVX__
          }
          else if (vop_set_cntr == (FRM_ARRAY_SIZE -1))
          {
            pckt_end_idx = MAX_NO_B_FRMS;
            pckt_ready = true;
#ifdef __DEBUG_DIVX__
            pckt_type[pckt_end_idx] = '\0';
            total_frames += pckt_end_idx;
#endif //__DEBUG_DIVX__
          }
          else
            vop_set_cntr++;
        }
        else
        {
          // The vop start code was found in the last 4 bytes,
          // seek backwards by 4 to include this start code
          // with the next buffer.
          lseek64(m_InputFileFd, -4, SEEK_CUR);
          byte_pos -= 4;
#ifdef __DEBUG_DIVX__
          pckd_frms--;
#endif //__DEBUG_DIVX__
        }
      }
    }
    if (pckt_ready)
    {
      loff_t off = (byte_pos - offset_array[pckt_end_idx]);
      if (lseek64(m_InputFileFd, -1LL*off , SEEK_CUR) == -1)
      {
        VLOGE("lseek64 with offset = %lld failed with errno %d"
            ", current position =0x%llx", -1LL*off,
            errno, lseek64(m_InputFileFd, 0, SEEK_CUR));
      }
    }
    else
    {
      char eofByte;
      int ret = read(m_InputFileFd, &eofByte, 1);
      if (ret == 0)
      {
        offset_array[vop_set_cntr] = byte_pos;
        pckt_end_idx = vop_set_cntr;
        pckt_ready = true;
#ifdef __DEBUG_DIVX__
        pckt_type[pckd_frms] = '\0';
        total_frames += pckd_frms;
#endif //__DEBUG_DIVX__
      }
      else if (ret == 1)
      {
        if (lseek64(m_InputFileFd, -1, SEEK_CUR) == -1)
        {
          VLOGE("lseek64 failed with errno = %d, "
              "current fileposition = %llx",
              errno,
              lseek64(m_InputFileFd, 0, SEEK_CUR));
        }
      }
      else
      {
        VLOGE("Error when checking for EOF");
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
  FUNCTION_EXIT();
  return pBufHdr->nFilledLen;
}

static int ReadBufferFromDivX311File(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  // TODO: we need to check if this timeStampLfile needed? or use global variant^M
  //static OMX_S64 timeStampLfile = 0;^M
  char *p_buffer = NULL;
  bool pkt_ready = false;
  unsigned int frame_type = 0;
  unsigned int bytes_read = 0;
  unsigned int frame_size = 0;
  unsigned int num_bytes_size = 4;
  unsigned int num_bytes_frame_type = 1;
  unsigned int n_offset = 0;
  FUNCTION_ENTER();

  if (pBufHdr != NULL)
  {
    p_buffer = (char *)pBufHdr->pBuffer + pBufHdr->nOffset;
  }
  else
  {
    VLOGD(" ERROR:Read_Buffer_From_DivX_311_File: pBufHdr is NULL");
    FUNCTION_EXIT();
    return 0;
  }

  n_offset = pBufHdr->nOffset;


  pBufHdr->nTimeStamp = timeStampLfile;

  if (p_buffer == NULL)
  {
    VLOGD(" ERROR:Read_Buffer_From_DivX_311_File: p_bufhdr is NULL");
    FUNCTION_EXIT();
    return 0;
  }

  //Read first frame based on size
  //DivX 311 frame - 4 byte header with size followed by the frame

  bytes_read = read(m_InputFileFd, &frame_size, num_bytes_size);

  VLOGD("Read_Buffer_From_DivX_311_File: Frame size = %d", frame_size);
  n_offset += read(m_InputFileFd, p_buffer, frame_size);

  pBufHdr->nTimeStamp = timeStampLfile;

  timeStampLfile += timestampInterval;

  //the packet is ready to be sent
  VLOGD("Returning Read Buffer from Divx 311: TS=[%ld], Offset=[%d]",
      (long int)pBufHdr->nTimeStamp,
      n_offset);

  FUNCTION_EXIT();
  return n_offset;
}
#endif //__DEBUG_DIVX__

int ReadBufferFromVP8File(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
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
  FUNCTION_ENTER();

  if (pBufHdr != NULL)
  {
    p_buffer = (char *)pBufHdr->pBuffer + pBufHdr->nOffset;
  }
  else
  {
    VLOGD(" ERROR: Read_Buffer_From_VP8_File: pBufHdr is NULL");
    FUNCTION_EXIT();
    return 0;
  }
  n_offset = pBufHdr->nOffset;

  if (p_buffer == NULL)
  {
    VLOGD(" ERROR: Read_Buffer_From_VP8_File: p_bufhdr is NULL");
    FUNCTION_EXIT();
    return 0;
  }

  if(ivf_header_read == 0)
  {
    bytes_read = read(m_InputFileFd, p_buffer, 32);
    ivf_header_read = 1;
    if(p_buffer[0] == 'D' && p_buffer[1] == 'K' && p_buffer[2] == 'I' && p_buffer[3] == 'F')
    {
      VLOGD("  IVF header found  ");
    }
    else
    {
      VLOGD("  No IVF header found  ");
      lseek(m_InputFileFd, -32, SEEK_CUR);
    }
  }
  bytes_read = read(m_InputFileFd, &frame_size, 4);
  bytes_read = read(m_InputFileFd, &time_stamp, 8);
  n_offset += read(m_InputFileFd, p_buffer, frame_size);
  pBufHdr->nTimeStamp = time_stamp;

  FUNCTION_EXIT();
  return n_offset;
}

// TODO Current impl has some problem when handling TP10_UBWC.
// display order can not handle the last 2 frames.
// Check the diff between the impl and read buffer func of VTEST.
static int ReadBufferFromH265StartCodeFile(OMX_BUFFERHEADERTYPE  *pBufHdr)
{
  int bytes_read = 0;
  int cnt = 0;
  unsigned int code = 0;
  int nalu_type = 0;
  int new_frame = 0;
  char *dataptr = (char *)pBufHdr->pBuffer;
  FUNCTION_ENTER();
  do
  {
    new_frame = 0;
    bytes_read = read(m_InputFileFd, &dataptr[cnt], 1);
    if (!bytes_read)
    {
      VLOGD("Bytes read Zero");
      break;
    }
    code <<= 8;
    code |= (0x000000FF & dataptr[cnt]);
    cnt++;
    if ((cnt == 4) && (code != H265_START_CODE))
    {
      VLOGE("ERROR: Invalid start code found 0x%x", code);
      cnt = 0;
      break;
    }
    if ((cnt > 4) && (code == H265_START_CODE))
    {
      VLOGD("Found H265_START_CODE");
      bytes_read = read(m_InputFileFd, &dataptr[cnt], 1);
      if (!bytes_read)
      {
        VLOGD("Bytes read Zero");
        break;
      }
      VLOGD("READ Byte[%d] = 0x%x", cnt, dataptr[cnt]);
      nalu_type = (dataptr[cnt] & HEVC_NAL_UNIT_TYPE_MASK) >> 1;
      cnt++;
      if (nalu_type <= HEVC_NAL_UNIT_TYPE_VCL_LIMIT &&
          nalu_type != HEVC_NAL_UNIT_TYPE_RSV_IRAP_VCL22 &&
          nalu_type != HEVC_NAL_UNIT_TYPE_RSV_IRAP_VCL23 &&
          nalu_type != HEVC_NAL_UNIT_TYPE_RSV_VCL_N10 &&
          nalu_type != HEVC_NAL_UNIT_TYPE_RSV_VCL_N12 &&
          nalu_type != HEVC_NAL_UNIT_TYPE_RSV_VCL_N14 &&
          nalu_type != HEVC_NAL_UNIT_TYPE_RSV_VCL_R11 &&
          nalu_type != HEVC_NAL_UNIT_TYPE_RSV_VCL_R13 &&
          nalu_type != HEVC_NAL_UNIT_TYPE_RSV_VCL_R15)
      {
        VLOGD("Found AU for HEVC");
        bytes_read = read(m_InputFileFd, &dataptr[cnt], 1);
        if (!bytes_read)
        {
          VLOGD("Bytes read Zero");
          break;
        }
        cnt ++;
        bytes_read = read(m_InputFileFd, &dataptr[cnt], 1);
        VLOGD("READ Byte[%d] = 0x%x firstsliceflag %d", cnt, dataptr[cnt], dataptr[cnt] >> 7);
        new_frame = (dataptr[cnt] >>7); //firstsliceflag
        cnt++;
        if (new_frame)
        {
          lseek64(m_InputFileFd, -7, SEEK_CUR);
          cnt -= 7;
          VLOGD("Found a NAL unit (type 0x%x) of size = %d",
              ((dataptr[4] & HEVC_NAL_UNIT_TYPE_MASK) >> 1), cnt);
          break;
        }
        else
        {
          VLOGD("Not a New Frame");
        }
      }
      else
      {
        lseek64(m_InputFileFd, -5, SEEK_CUR);
        cnt -= 5;
        VLOGD("Found NAL unit (type 0x%x) of size = %d",
            ((dataptr[4] & HEVC_NAL_UNIT_TYPE_MASK) >> 1), cnt);
        break;
      }
    }
  } while (1);

  timeStampLfile += timestampInterval;

  FUNCTION_EXIT();
  return cnt;
}

bool SetReadBufferType(int32_t file_type_option, int32_t codec_format_option)
{
  FUNCTION_ENTER();
  if (file_type_option == FILE_TYPE_DAT_PER_AU)
  {
    ReadBuffer = ReadBufferFromDATFile;
  }
  else if (file_type_option == FILE_TYPE_ARBITRARY_BYTES)
  {
    ReadBuffer = ReadBufferArbitraryBytes;
  }
  else if (codec_format_option == OMX_VIDEO_CodingAVC)
  {
    if (file_type_option == FILE_TYPE_264_NAL_SIZE_LENGTH)
    {
      ReadBuffer = ReadBufferFromSizeNal;
    }
    else if (file_type_option == FILE_TYPE_264_START_CODE_BASED)
    {
      ReadBuffer = ReadBufferFromH264StartCodeFile;
    }
    else
    {
      VLOGE("Invalid file_type_option(%d) for H264", file_type_option);
      FUNCTION_EXIT();
      return false;
    }
  }
  else if(codec_format_option == OMX_VIDEO_CodingHEVC)
  {
    if (file_type_option == FILE_TYPE_265_START_CODE_BASED)
    {
      ReadBuffer = ReadBufferFromH265StartCodeFile;
    }
    else
    {
      VLOGD("Invalid file_type_option(%d) for HEVC", file_type_option);
      FUNCTION_EXIT();
      return -1;
    }
  }
  else if ((codec_format_option == OMX_VIDEO_CodingH263) ||
      (codec_format_option == OMX_VIDEO_CodingMPEG4))
  {
    ReadBuffer = ReadBufferFromVopStartCodeFile;
  }
  else if (codec_format_option == OMX_VIDEO_CodingMPEG2)
  {
    ReadBuffer = ReadBufferFromMpeg2StartCode;
  }
#ifdef __DEBUG_DIVX__
  else if (file_type_option == FILE_TYPE_DIVX_4_5_6)
  {
    ReadBuffer = ReadBufferFromDivX456File;
  }
  else if (file_type_option == FILE_TYPE_DIVX_311)
  {
    ReadBuffer = ReadBufferFromDivX311File;
  }
#endif //__DEBUG_DIVX__
  else if (file_type_option == FILE_TYPE_RCV)
  {
    ReadBuffer = ReadBufferFromVC1RCVFile;
  }
  else if (file_type_option == FILE_TYPE_VP8)
  {
    ReadBuffer = ReadBufferFromVP8File;
  }
  else if (file_type_option == FILE_TYPE_VC1)
  {
    ReadBuffer = ReadBufferFromVC1RCVFile;
  } 
  else
  {
    VLOGE("Invalid file_type_option(%d).", file_type_option);
    FUNCTION_EXIT();
    return false;
  }
  FUNCTION_EXIT();
  return true;
}

void WaitEventComplete(void)
{
  FUNCTION_ENTER();
  pthread_mutex_lock(&m_EventCompleteMutex);
  while (m_EventComplete == 0)
  {
    pthread_cond_wait(&m_EventCompleteSignal, &m_EventCompleteMutex);
  }
  m_EventComplete = 0;
  pthread_mutex_unlock(&m_EventCompleteMutex);

  FUNCTION_EXIT();
}

void NotifyEventComplete(void)
{
  FUNCTION_ENTER();
  pthread_mutex_lock(&m_EventCompleteMutex);
  if (m_EventComplete == 0)
  {
    m_EventComplete = 1;
    pthread_cond_signal(&m_EventCompleteSignal);
  }
  pthread_mutex_unlock(&m_EventCompleteMutex);
  FUNCTION_ENTER();
}

void NotifyToStop()
{
  FUNCTION_ENTER();

  pthread_mutex_lock(&m_RunningMutex);
  m_Running = false;
  pthread_cond_signal(&m_RunningSignal);
  pthread_mutex_unlock(&m_RunningMutex);

  FUNCTION_EXIT();
}

OMX_ERRORTYPE EventCallback(OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_EVENTTYPE eEvent,
    OMX_IN OMX_U32 nData1, OMX_IN OMX_U32 nData2,
    OMX_IN OMX_PTR pEventData)
{
  OMX_ERRORTYPE result = OMX_ErrorNone;

  FUNCTION_ENTER();

  switch(eEvent)
  {
    case OMX_EventCmdComplete:
      VLOGD(" OMX_EventCmdComplete ");
      if(OMX_CommandPortDisable == (OMX_COMMANDTYPE)nData1)
      {
        VLOGD("*********************************************");
        VLOGD("Recieved DISABLE Event Command Complete[%d]",nData2);
        VLOGD("*********************************************");
        VLOGD("Output port disabled");
        NotifyEventComplete();
      }
      else if(OMX_CommandPortEnable == (OMX_COMMANDTYPE)nData1)
      {
        VLOGD("*********************************************");
        VLOGD("Recieved ENABLE Event Command Complete[%d]",nData2);
        VLOGD("*********************************************");
        pthread_mutex_lock(&m_ReconfigMutex);
        if (m_NeedReconfigPort)
        {
          VLOGD("Output port enabled");
          m_NeedReconfigPort = false;
          pthread_cond_signal(&m_ReconfigSignal);
        }
        pthread_mutex_unlock(&m_ReconfigMutex);
        NotifyEventComplete();
      }
      else if(OMX_CommandFlush == (OMX_COMMANDTYPE)nData1)
      {
        VLOGD("*********************************************");
        VLOGD("Received FLUSH Event Command Complete[%d]",nData2);
        VLOGD("*********************************************");
        if (nData2 == 0)
        {
          VLOGD("FLUSH input Done");
        }
        else if (nData2 == 1)
        {
          VLOGD("FLUSH output Done");
        }
      }
      else if (OMX_CommandStateSet == (OMX_COMMANDTYPE)nData1)
      {
        VLOGD("*********************************************");
        VLOGD("Received STATE SET Event Command Complete[%d]",nData2);
        VLOGD("*********************************************");
        StateEvent_t newState;
        newState.event = eEvent;
        newState.cmd = nData1;
        newState.cmdData = nData2;
        PushEvent(newState);
      }
      else
      {
        VLOGD("unknown nData1 = %d", nData1);
      }
      // even reconfig occurred, flush will NOT be triggered automatically in lower layer.
      break;

    case OMX_EventError:
      VLOGE("*********************************************");
      VLOGE("Received OMX_EventError Event Command !");
      VLOGE("*********************************************");
      if (OMX_ErrorInvalidState == (OMX_ERRORTYPE)nData1)
      {
        VLOGE("Invalid State error ");
      }
      if (OMX_ErrorHardware == (OMX_ERRORTYPE)nData1)
      {
        VLOGE("Hardware error ");
      }
      break;
    case OMX_EventPortSettingsChanged:
      VLOGD("*********************************************");
      VLOGD("Received OMX_EventPortSettingsChanged on port[%d]", nData1);
      VLOGD("*********************************************");
      if (nData2 == OMX_IndexConfigCommonOutputCrop)
      {

        if (nData1 == PORT_INDEX_OUT)
        {
          crop_rect.nPortIndex = PORT_INDEX_OUT;
          crop_rect.nSize = sizeof(OMX_CONFIG_RECTTYPE);
          result = OMX_GetConfig(m_Handle,
              OMX_IndexConfigCommonOutputCrop, &crop_rect);
          if (OMX_ErrorNone != result)
          {
            VLOGE("Failed to get crop rectangle");
            break;
          }
          else
            VLOGD("Got Crop Rect: (%d, %d) (%d x %d)",
                crop_rect.nLeft, crop_rect.nTop, crop_rect.nWidth, crop_rect.nHeight);
        }
        else
        {
          VLOGD("nData2: %d, Port: %s", nData2, (nData1 == PORT_INDEX_IN ? "IN" : "OUT"));
        }
        break;
      }

      if (nData2 != OMX_IndexParamPortDefinition)
        break;
      if (waitForFirstTimeReconfigEvent)
      {
        // only-one-time flag triggered
        // This flag is for skip the first time receiving portsettingschange event.
        //waitForFirstTimeReconfigEvent = 0;
        m_NeedReconfigPort = true;
        NotifyEventComplete();
      }
      else
      {
        m_NeedReconfigPort = true;
        pthread_mutex_lock(&m_RunningMutex);
        pthread_cond_signal(&m_RunningSignal);
        pthread_mutex_unlock(&m_RunningMutex);
      }

      break;

  }
  VLOGD("Recv event: %d, data1: %d, data2: %d", eEvent, nData1, nData2);

  if (eEvent == OMX_EventError)
  {
    VLOGE("Recv error event, goto stop");
    NotifyToStop();
    FUNCTION_EXIT();
    return result;
  }
  FUNCTION_EXIT();
  return result;
}

OMX_ERRORTYPE EmptyBufferDoneCallback(OMX_IN OMX_HANDLETYPE hComponent,
                              OMX_IN OMX_PTR pAppData,
                              OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
  FUNCTION_ENTER();

  VLOGD("Input buffer (%p), filled length (%d), time stamp (%lld)",
      pBuffer, pBuffer->nFilledLen, pBuffer->nTimeStamp);
  BufferMessage_t bufferMsg;
  bufferMsg.pBuffer = pBuffer;
  PushInputEmptyBuffer(bufferMsg);

  FUNCTION_EXIT();
  return OMX_ErrorNone;
}

OMX_ERRORTYPE FillBufferDoneCallback(OMX_OUT OMX_HANDLETYPE hComponent,
                             OMX_OUT OMX_PTR pAppData,
                             OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {
  FUNCTION_ENTER();

  if (!m_OutputThreadRunning) {
    FUNCTION_EXIT();
    return OMX_ErrorNone;
  }

  VLOGD("Output buffer (%p), filled length (%d), time stamp (%lld), flag (%#X)",
      pBuffer, pBuffer->nFilledLen, pBuffer->nTimeStamp, pBuffer->nFlags);

  gettimeofday(&m_DecodeFrameEndTime[m_FillFramNum % OMX_BUFFERS_NUM], NULL);
  timeval decode_frame_start_time = m_DecodeFrameStartTime[m_FillFramNum % OMX_BUFFERS_NUM];
  timeval decode_frame_end_time = m_DecodeFrameEndTime[m_FillFramNum % OMX_BUFFERS_NUM];
  if (decode_frame_start_time.tv_sec != 0) {
    int64_t decode_frame_time = (decode_frame_end_time.tv_sec - decode_frame_start_time.tv_sec) * 1000 + \
             (decode_frame_end_time.tv_usec - decode_frame_start_time.tv_usec) / 1000;
    if (m_DecodeFrameTimeMax < decode_frame_time) {
      m_DecodeFrameTimeMax = decode_frame_time;
    }

    if (m_DecodeFrameTimeMin > decode_frame_time || m_DecodeFrameTimeMin == 0) {
      m_DecodeFrameTimeMin = decode_frame_time;
    }

    VLOGP("decode one frame cost time max: %5d ms, min %5d ms", m_DecodeFrameTimeMax, m_DecodeFrameTimeMin);
    m_DecodeTotalTimeActal += decode_frame_time;
  }

  m_FillFramNum++;

  BufferMessage_t bufferMsg;
  bufferMsg.pBuffer = pBuffer;
  PushOutputFilledBuffer(bufferMsg);
  m_FreedOutputBufferCnt++;

  FUNCTION_EXIT();
  return OMX_ErrorNone;
}

bool InitializeCodec(OMX_U32 eCodec, int32_t filetype)
{
  OMX_ERRORTYPE result = OMX_ErrorNone;
  OMX_CALLBACKTYPE callbacks = {EventCallback, EmptyBufferDoneCallback, FillBufferDoneCallback};
  FUNCTION_ENTER();

  if (eCodec >= OMX_VIDEO_CodingMax)
  {
    VLOGE("invalid parameter: eCodec: %d", eCodec);
    FUNCTION_EXIT();
    return false;
  }

  pthread_mutex_init(&m_RunningMutex, NULL);
  pthread_cond_init(&m_RunningSignal, NULL);
  m_Running = true;
  pthread_mutex_init(&m_EventCompleteMutex, NULL);
  pthread_cond_init(&m_EventCompleteSignal, NULL);
  pthread_mutex_init(&m_ReconfigMutex, NULL);
  pthread_cond_init(&m_ReconfigSignal, NULL);

  if (OpenVideoFile() < 0)
  {
    VLOGE("open i/p file failed!");
    FUNCTION_EXIT();
    return false;
  }

  InitQueue();
  result = OMX_Init();
  CHECK_RESULT("OMX_Init failed:", result);

  m_Handle = NULL;
  const char * componentName = GetComponentRole(eCodec, filetype);
  if (componentName == NULL)
  {
    VLOGE("Error: Unsupported codec %d", eCodec);
    FUNCTION_EXIT();
    return false;
  }
  VLOGD("Get codec handle: %s", componentName);

  CheckIsSWCodec(componentName);

  result = OMX_GetHandle((OMX_HANDLETYPE*)(&m_Handle),
      (OMX_STRING)componentName, NULL, &callbacks);
  if (m_Handle == NULL) {
    VLOGE("Failed to get codec handle.");
    FUNCTION_EXIT();
    return false;
  }
  CHECK_RESULT("Failed to get codec handle", result);

  QOMX_VIDEO_QUERY_DECODER_INSTANCES decoder_instances;
  OMX_INIT_STRUCT(&decoder_instances, QOMX_VIDEO_QUERY_DECODER_INSTANCES);
  result = OMX_GetConfig(m_Handle,
      (OMX_INDEXTYPE)OMX_QcomIndexQueryNumberOfVideoDecInstance,
      &decoder_instances);
  CHECK_RESULT("Failed to get config", result);
  VLOGD("Number of decoder instances %d.", decoder_instances.nNumOfInstances);

  OMX_PORT_PARAM_TYPE portParam;
  OMX_INIT_STRUCT(&portParam, OMX_PORT_PARAM_TYPE);
  result = OMX_GetParameter(m_Handle, OMX_IndexParamVideoInit, (OMX_PTR)&portParam);
  CHECK_RESULT("get OMX_IndexParamVideoInit info", result);
  VLOGD("ports: %d, StartPortNumber: %d", portParam.nPorts, portParam.nStartPortNumber);

  FUNCTION_EXIT();
  return true;
}

void CheckIsSWCodec(const char * component)
{
  FUNCTION_ENTER();

  char tmp[2];
  strlcpy(tmp, component + strlen(component) - 2, 4);
  if (!strncmp("sw", tmp, 2))
  {
    m_IsSWCodec = true;
  }
  else
  {
    m_IsSWCodec = false;
  }
  VLOGD("It's %s Codec", (m_IsSWCodec ? "SW" : "HW"));

  FUNCTION_EXIT();
}

void CheckFpsControl(OMX_U32 fps)
{
  if (m_FpsControl)
  {
    m_DecodeDuration = 1000 / fps * 1000;
    VLOGD("FPS control: fps = %d, decode duration = %ld", fps, m_DecodeDuration);
  }
}

void CheckIfNeedToWaitReconfig(OMX_U32 height, OMX_U32 width, OMX_U32 eCodec)
{
  FUNCTION_ENTER();

  switch (eCodec)
  {
    case OMX_VIDEO_CodingAVC:
    case OMX_VIDEO_CodingMPEG2:
    case OMX_VIDEO_CodingHEVC:
      if (height % DEFAULT_WIDTH_ALIGNMENT == 0 && width % DEFAULT_HEIGHT_ALIGNMENT == 0)
        waitForFirstTimeReconfigEvent = 0;
      else
        waitForFirstTimeReconfigEvent = 1;
      if (QOMX_COLOR_FORMATYUV420SemiPlanarP010Venus == settings->nColorFormat ||
          QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m10bitCompressed == settings->nColorFormat)
        waitForFirstTimeReconfigEvent = 1;
      VLOGD("Check the WxH and we will%shave a reconfig at the beginning",
          (waitForFirstTimeReconfigEvent == 1 ? " " : " NOT "));
      break;
    case OMX_VIDEO_CodingVP8:
    case OMX_VIDEO_CodingVP9:
      // Move VP8 VP9 here, since 4096x2160 Not need reconfig, but 1920x1080 need
    case OMX_VIDEO_CodingH263:
    case OMX_VIDEO_CodingMPEG4:
    case QOMX_VIDEO_CodingDivx:
    case OMX_VIDEO_CodingWMV:
      // for SW codecs H263, MPEG4, maybe DIVX and WMV in the future, reconfig as default
      waitForFirstTimeReconfigEvent = 1;
      VLOGD("SW codec need reconfig as default");
      break;
    default:
      VLOGE("Error: Unknown codec %d, skip check", eCodec);
  }

  FUNCTION_EXIT();
}

bool CheckColorFormatSupported(OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFmt, OMX_U32 color)
{
  OMX_ERRORTYPE result = OMX_ErrorNone;
  int index = 0;
  FUNCTION_ENTER();

  while (result == OMX_ErrorNone)
  {
    pVideoPortFmt->nPortIndex = PORT_INDEX_OUT;
    pVideoPortFmt->nIndex = index;
    result = OMX_GetParameter(m_Handle, OMX_IndexParamVideoPortFormat,
        (OMX_PTR)pVideoPortFmt);

    if((result == OMX_ErrorNone) && (pVideoPortFmt->eColorFormat == color))
    {
      VLOGD("Decoder: Format[%u] supported by OMX Decoder.", pVideoPortFmt->eColorFormat);
      break;
    }
    index++;
  }
  CHECK_RESULT("Output get OMX_IndexParamVideoPortFormat error", result);
  FUNCTION_EXIT();
  return true;
}

bool SetInPortParameters(OMX_U32 filetype, OMX_U32 height, OMX_U32 width, OMX_U32 fps, OMX_U32 eCodec)
{
  OMX_ERRORTYPE result = OMX_ErrorNone;
  FUNCTION_ENTER();

  OMX_PARAM_PORTDEFINITIONTYPE portdef;
  OMX_INIT_STRUCT(&portdef, OMX_PARAM_PORTDEFINITIONTYPE);
  portdef.nPortIndex = (OMX_U32) PORT_INDEX_IN;

  result = OMX_GetParameter(m_Handle,OMX_IndexParamPortDefinition,&portdef);
  CHECK_RESULT("get OMX_IndexParamPortDefinition error", result);

  VLOGD("Decoder: Input min buffer count %u", (unsigned int)portdef.nBufferCountMin);
  VLOGD("Decoder: Input buffer size %u", (unsigned int)portdef.nBufferSize);

  m_InputBufferSize = portdef.nBufferSize;
  m_InputBufferCount = portdef.nBufferCountActual;
  VLOGD("m_InputBufferSize = %d, m_InputBufferCount = %d", m_InputBufferSize , m_InputBufferCount);
  if(OMX_DirInput != portdef.eDir)
  {
    VLOGE("Error: Expect input port");
    FUNCTION_EXIT();
    return false;
  }

  portdef.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)eCodec;
  // We got codec fmt before at InitializeCodec into m_portFmt already
  portdef.format.video.nFrameHeight = height;
  portdef.format.video.nFrameWidth  = width;
  FractionToQ16(portdef.format.video.xFramerate, fps);
  //portdef.format.video.xFramerate = fps;
  result = OMX_SetParameter(m_Handle, OMX_IndexParamPortDefinition, &portdef);
  CHECK_RESULT("Input set OMX_IndexParamPortDefinition error: Height, Width, FPS", result);

  result = OMX_GetParameter(m_Handle, OMX_IndexParamPortDefinition, &portdef);
  CHECK_RESULT("Input get OMX_IndexParamPortDefinition error", result);
  VLOGD("Decoder: Input actual output buffer count %u", (unsigned int)portdef.nBufferCountActual);
  VLOGD("Video format: W x H (%d x %d)", portdef.format.video.nFrameWidth,
      portdef.format.video.nFrameHeight);

  crop_rect.nLeft = 0;
  crop_rect.nTop = 0;
  // for VC1/RCV, crop_rect width/height will be modified
  // following read out data from file.
  crop_rect.nWidth = width;
  crop_rect.nHeight = height;
  // Video format above should be the same with crop_rect W/H and W/H.

  FUNCTION_EXIT();
  return true;
}

bool SetOutPortParameters()
{
  //OMX_VIDEO_PARAM_PORTFORMATTYPE videoPortFmt;
  OMX_ERRORTYPE result = OMX_ErrorNone;

  FUNCTION_ENTER();

  // check current port fmt
  OMX_PARAM_PORTDEFINITIONTYPE portdef;
  OMX_INIT_STRUCT_SIZE(&portdef, OMX_PARAM_PORTDEFINITIONTYPE);
  portdef.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  result = OMX_GetParameter(m_Handle, OMX_IndexParamPortDefinition, &portdef);
  CHECK_RESULT("get OMX_IndexParamPortDefinition error", result);

  VLOGD("Decoder: Output min buffer count %u", (unsigned int)portdef.nBufferCountMin);
  VLOGD("Decoder: Output buffer size %u", (unsigned int)portdef.nBufferSize);
  VLOGD("Decoder: Output actual output buffer count %u", (unsigned int)portdef.nBufferCountActual);
  m_OutputBufferSize = portdef.nBufferSize;
  m_OutputBufferCount = portdef.nBufferCountActual;
  m_FreedOutputBufferCnt = m_OutputBufferCount;
  if (OMX_DirOutput != portdef.eDir)
  {
    VLOGE("Error: Expect output port");
    FUNCTION_EXIT();
    return false;
  }

  FUNCTION_EXIT();
  return true;
}

bool SetPictureOrder(int32_t pic_order)
{
  OMX_ERRORTYPE result = OMX_ErrorNone;
  FUNCTION_ENTER();

  QOMX_VIDEO_DECODER_PICTURE_ORDER picture_order;
  OMX_INIT_STRUCT(&picture_order, QOMX_VIDEO_DECODER_PICTURE_ORDER);
  picture_order.eOutputPictureOrder = QOMX_VIDEO_DISPLAY_ORDER;
  if (pic_order == 1)
  {
    picture_order.eOutputPictureOrder = QOMX_VIDEO_DECODE_ORDER;
  }

  picture_order.nPortIndex = PORT_INDEX_OUT;
  result = OMX_SetParameter(m_Handle, (OMX_INDEXTYPE)OMX_QcomIndexParamVideoDecoderPictureOrder,
      (OMX_PTR)&picture_order);
  CHECK_RESULT("Output set OMX_QcomIndexParamVideoDecoderPictureOrder error: Pic Order", result);

  FUNCTION_EXIT();
  return true;
}

#ifdef PRE_SDM855
bool SetNalSize(int32_t nalSize)
{
  OMX_ERRORTYPE result = OMX_ErrorNone;
  FUNCTION_ENTER();

  // Set NAL size for H264,H265
  OMX_VIDEO_CONFIG_NALSIZE cfgNalSize;
  OMX_INIT_STRUCT(&cfgNalSize, OMX_VIDEO_CONFIG_NALSIZE);
  cfgNalSize.nNaluBytes = nalSize;
  VLOGD("Decoder: set Nal size %d index %d", nalSize, OMX_IndexConfigVideoNalSize);

  result = OMX_SetConfig(m_Handle, OMX_IndexConfigVideoNalSize, (OMX_PTR)&cfgNalSize);
  CHECK_RESULT("Set OMX_IndexConfigVideoNalSize error: nal size", result);

  FUNCTION_EXIT();
  return true;
}
#endif

// codec is not used.
bool RegisterBuffer(int32_t height, int32_t width, int colorformat, int codec, PortIndexType port)
{
  OMX_ERRORTYPE result = OMX_ErrorNone;
  FUNCTION_ENTER();

  int32_t i = 0;
  if (port == PORT_INDEX_IN)
  {
    if (m_InputBufferSize == 0 ||
        m_InputBufferCount == 0 ||
        m_InputBufferCount > OMX_BUFFERS_NUM)
    {
      VLOGE("Invalid buffer parameter: input:%d/%d, count:%d",
          m_InputBufferSize, m_InputBufferCount,
          OMX_BUFFERS_NUM);
      FUNCTION_EXIT();
      return false;
    }


    // input buffer
    if (!m_IsSWCodec)
    {
      for (i = 0; i < m_InputBufferCount; i++)
      {
        OMX_U8* pBuff;
        pBuff = (OMX_U8*)malloc(m_InputBufferSize);
        result = OMX_UseBuffer(m_Handle,
            &m_InputBufferHeaders[i],
            (OMX_U32) PORT_INDEX_IN,
            NULL,
            m_InputBufferSize,
            pBuff);
        CHECK_RESULT("Use input buffer failed", result);
      }
    }
    else
    {
      for (i = 0; i < m_InputBufferCount; i++)
      {
        //we allocate buffer for SW codecs
        result = OMX_AllocateBuffer(m_Handle,
            &m_InputBufferHeaders[i],
            (OMX_U32) PORT_INDEX_IN,
            NULL,
            m_InputBufferSize);
        CHECK_RESULT("Allocate input buffer failed", result);
      }
    }
  }

  if (port == PORT_INDEX_OUT)
  {
    if (m_OutputBufferSize == 0 ||
        m_OutputBufferCount == 0 ||
        m_OutputBufferCount > OMX_BUFFERS_NUM)
    {
      VLOGE("Invalid buffer parameter: output:%d/%d, count:%d",
          m_OutputBufferSize, m_OutputBufferCount,
          OMX_BUFFERS_NUM);
      FUNCTION_EXIT();
      return false;
    }
    for (i = 0; i < m_OutputBufferCount; i++)
    {
      result = OMX_AllocateBuffer(m_Handle,
          &m_OutputBufferHeaders[i],
          (OMX_U32) PORT_INDEX_OUT,
          NULL,
          m_OutputBufferSize);
      CHECK_RESULT("Allocate output buffer failed", result);
    }
  }
  FUNCTION_EXIT();
  return true;
}

const char * TransferStateToStr(OMX_U32 state)
{
  FUNCTION_ENTER();
  char * str_state;
  switch (state) {
    case OMX_StateLoaded:
      str_state = "OMX_StateLoaded";
    case OMX_StateIdle:
      str_state = "OMX_StateIdle";
    case OMX_StateExecuting:
      str_state = "OMX_StateExecuting";
    case OMX_StateInvalid:
      str_state = "OMX_StateInvalid";
    case OMX_StateWaitForResources:
      str_state = "OMX_StateWaitForResources";
    case OMX_StatePause:
      str_state = "OMX_StatePause";
    default:
      str_state = "Not Support this state";
  }
  FUNCTION_EXIT();
  return str_state;
}

bool WaitState(OMX_STATETYPE eState)
{
  FUNCTION_ENTER();

  VLOGD("waiting state: %s", TransferStateToStr(eState));

  // wait component return set result
  StateEvent_t newState;
  bool ret = PopEvent(&newState);
  if (ret == false) {
    VLOGE("Waiting for state:%s failed", TransferStateToStr(eState));
    FUNCTION_EXIT();
    return false;
  }

  if (newState.event == OMX_EventCmdComplete &&
      newState.cmd == OMX_CommandStateSet &&
      newState.cmdData == eState)
  {
    VLOGD("wait state: %s Success", TransferStateToStr(eState));
    VLOGD("event: %d, cmd: %d, cmdData: %d",
        newState.event,
        newState.cmd,
        newState.cmdData);
    m_eState = eState;
    ret = true;
  }
  else
  {
    VLOGE("recv bad event: %d, cmd: %d, cmdData: %d",
        newState.event,
        newState.cmd,
        newState.cmdData);
    ret = false;
  }

  FUNCTION_EXIT();
  return ret;
}

bool SetState(OMX_STATETYPE eState, OMX_BOOL sync)
{
  OMX_ERRORTYPE result = OMX_ErrorUndefined;
  bool ret = true;
  FUNCTION_ENTER();

  VLOGD("current:%s, pending: %s, new: %s, sync: %d",
      TransferStateToStr(m_eState),
      TransferStateToStr(m_PendingEstate),
      TransferStateToStr(eState),
      sync);

  if (m_eState == eState)
  {
    VLOGD("set old state:%s, skip", TransferStateToStr(eState));
    FUNCTION_EXIT();
    return true;
  }

  if (m_PendingEstate != m_eState)
  {
    ret = WaitState(m_PendingEstate);
    CHECK_BOOL("waitState failed", ret);  // if ret == false, exit
  }

  if ((eState == OMX_StateLoaded && m_eState != OMX_StateIdle) ||
      (eState == OMX_StateExecuting && m_eState != OMX_StateIdle))
  {
    VLOGE("Invalid new state: %d, current state: %d",
        eState, m_eState);
    FUNCTION_EXIT();
    return false;
  }

  // send new state to component
  VLOGD("Goto new state: %s, sync:%d", TransferStateToStr(eState), sync);
  result = OMX_SendCommand(m_Handle, OMX_CommandStateSet,
      (OMX_U32) eState, NULL);
  CHECK_RESULT("set new state failed:", result);

  m_PendingEstate = eState;

  if (sync == true)
  {
    ret = WaitState(m_PendingEstate);
    CHECK_BOOL("wait sync state failed", ret);
  }

  FUNCTION_EXIT();
  return ret;
}

bool ConfigureCodec(VideoCodecSetting_t *codecSettings)
{
  bool result = false;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  FUNCTION_ENTER();
  CHECK_HANDLE(m_Handle);
  settings = codecSettings;

  CheckFpsControl(codecSettings->nFrameRate);
  CheckIfNeedToWaitReconfig(codecSettings->nFrameHeight,
      codecSettings->nFrameWidth, codecSettings->eCodec);

  // set input port
  result = SetInPortParameters(codecSettings->nFileType, codecSettings->nFrameHeight,
      codecSettings->nFrameWidth, codecSettings->nFrameRate, codecSettings->eCodec);
  CHECK_BOOL("Set input port params error", result);

  // P010 was not supported on LV.
  /*if (QOMX_COLOR_FORMATYUV420SemiPlanarP010Venus == codecSettings->nColorFormat)
  {
    m_ConfigureColorFormat = QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m;
  }*/
  if (QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m10bitCompressed == codecSettings->nColorFormat)
  {
    m_ConfigureColorFormat = QOMX_COLOR_FORMATYUV420PackedSemiPlanar32mCompressed;
    VLOGD("TP10_UBWC -> NV12_UBWC");
  }
  else
  {
    m_ConfigureColorFormat = codecSettings->nColorFormat;
  }

  // check color format
  OMX_VIDEO_PARAM_PORTFORMATTYPE videoPortFmt = {0};
  OMX_INIT_STRUCT(&videoPortFmt, OMX_VIDEO_PARAM_PORTFORMATTYPE);
  result = CheckColorFormatSupported(&videoPortFmt, m_ConfigureColorFormat);
  CHECK_BOOL("Error: No Venus specific color format[0x%x] supported", result);

  // after confirmed color fmt, set back without any change
  ret = OMX_SetParameter(m_Handle, OMX_IndexParamVideoPortFormat,(OMX_PTR)&videoPortFmt);
  CHECK_RESULT("Output set OMX_IndexParamVideoPortFormat error: Color Format", ret);

  // set PictureOrder
  result = SetPictureOrder(codecSettings->nPictureOrder);
  CHECK_BOOL("set OMX_QcomIndexParamVideoDecoderPictureOrder error: picture order", result);

  #ifdef PRE_SDM855
  // Set NAL size for H264,H265 START
  if(codecSettings->eCodec == OMX_VIDEO_CodingAVC)
  {
    if (codecSettings->nFileType == FILE_TYPE_264_NAL_SIZE_LENGTH)
    {
      result = SetNalSize(codecSettings->nNalSize);
      CHECK_BOOL("Set nal size failed", result);
    }
  }
  #endif

  // set state to idle before register buffer and do not wait
  result = SetState(OMX_StateIdle, OMX_FALSE);
  CHECK_BOOL("Set state to Idle failed", result);

  // register input buffer
  result = RegisterBuffer(codecSettings->nFrameHeight, codecSettings->nFrameWidth,
                            codecSettings->nColorFormat, codecSettings->eCodec, PORT_INDEX_IN);
  CHECK_BOOL("register i/p buffer failed", result);

   // set output port
  result = SetOutPortParameters();
  CHECK_BOOL("Set output port params error", result);

  // register output buffer
  result = RegisterBuffer(codecSettings->nFrameHeight, codecSettings->nFrameWidth,
                            codecSettings->nColorFormat, codecSettings->eCodec, PORT_INDEX_OUT);
  CHECK_BOOL("register o/p buffer failed", result);

  FUNCTION_EXIT();
  return true;
}

bool StoreDecodedDataToFile(unsigned char * pBuffer, int32_t dataLen)
{
  bool status = false;
  int32_t writeDataLen = 0;

  FUNCTION_ENTER();

  if (pBuffer == NULL || dataLen <= 0)
  {
    VLOGE("bad parameters: pBuffer: %p, dataLen: %d", pBuffer, dataLen);
    FUNCTION_EXIT();
    return false;
  }

  // open output file
  if (m_OutputFileFd < 0)
  {
    if (m_OutputFileName != NULL)
    {
      m_OutputFileFd = open(m_OutputFileName, O_WRONLY|O_CREAT,
          S_IRWXU | S_IRWXG | S_IRWXO);  // 0x0777
      if (m_OutputFileFd < 0)
      {
        VLOGE("Can not open file: %s, error: %s",
            m_OutputFileName, strerror(errno));
        FUNCTION_EXIT();
        return false;
      }
    }
    else
    {
      VLOGE("No Output file");
      FUNCTION_EXIT();
      return false;
    }
  }

  if (settings->nColorFormat == QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m)
  {
    unsigned int stride = VENUS_Y_STRIDE(COLOR_FMT_NV12, settings->nFrameWidth);
    unsigned int scanlines = VENUS_Y_SCANLINES(COLOR_FMT_NV12, settings->nFrameHeight);
    unsigned char *temp = pBuffer;
    int i = 0;
    // This seems to finding the correct addr ptr of target buffer
    temp += (stride * (int)crop_rect.nTop) + (int)crop_rect.nLeft;
    for (i = 0; i < crop_rect.nHeight; i++)
    {
      writeDataLen += write(m_OutputFileFd, temp, crop_rect.nWidth); //count the writen length in loop
      temp += stride;
    }

    temp = pBuffer + stride * scanlines;
    temp += (stride * (int)crop_rect.nTop) + (int)crop_rect.nLeft;
    for(i = 0; i < crop_rect.nHeight/2; i++)
    {
      writeDataLen += write(m_OutputFileFd, temp, crop_rect.nWidth);
      temp += stride;
    }
  }
  // Not Supported
  /*else if (settings->nColorFormat == QOMX_COLOR_FORMATYUV420SemiPlanarP010Venus)
  {
    unsigned int stride = VENUS_Y_STRIDE(COLOR_FMT_P010, settings->nFrameWidth);
    unsigned int scanlines = VENUS_Y_SCANLINES(COLOR_FMT_P010, settings->nFrameHeight);
    unsigned char *temp = pBuffer;
    int i = 0;
    // This seems to finding the correct addr ptr of target buffer
    temp += (stride * (int)crop_rect.nTop) + (int)crop_rect.nLeft;
    for (i = 0; i < crop_rect.nHeight; i++)
    {
      writeDataLen += write(m_OutputFileFd, temp, crop_rect.nWidth * 2);
      temp += stride;
    }

    temp = pBuffer + stride * scanlines;
    temp += (stride * (int)crop_rect.nTop) + (int)crop_rect.nLeft;
    for(i = 0; i < crop_rect.nHeight/2; i++)
    {
      writeDataLen += write(m_OutputFileFd, temp, crop_rect.nWidth * 2);
      temp += stride;
    }
    VLOGD("Avalibale data %d bytes, write date %d bytes.", dataLen, writeDataLen);
  }*/
  else
  {
    //For UBWC and TP10_UBWC, write to file directly
    writeDataLen = write(m_OutputFileFd, pBuffer, dataLen);
    if (writeDataLen != dataLen)
    {
      VLOGE("Write buffer failed, write:%d, ret: %d, error: %s",
          dataLen, writeDataLen, strerror(errno));

      FUNCTION_EXIT();
      return false;
    }
  }
  VLOGD("Filled length %d bytes, write date %d bytes.", dataLen, writeDataLen);

  m_OutputDataSize += writeDataLen;
  FUNCTION_EXIT();
  return true;
}

bool StoreDecodedData(unsigned char * pBuffer, int32_t dataLen) {
  FUNCTION_ENTER();

  bool status = false;
  switch (m_TestMode) {
    case MODE_FILE_DECODE:
      status = StoreDecodedDataToFile(pBuffer, dataLen);
      break;
    default:
      VLOGE("bad test mode: %d", m_TestMode);
  }

  FUNCTION_EXIT();
  return status;
}

static void * InputBufferProcessThread(void *arg)
{
  int32_t i = 0;
  int32_t readFrameLen = 0;
  OMX_ERRORTYPE result = OMX_ErrorUndefined;
  int32_t retry = 0;
  bool bVc1FirstRead = true;
  OMX_TICKS nTimeStamp = 0;
  OMX_BOOL bWriteTimestamp = OMX_TRUE;

  FUNCTION_ENTER();

  m_InputThreadRunning = true;

  while (m_InputThreadRunning == true && retry < MAX_WAIT_TIMES)
  {
    BufferMessage_t bufferMsg;

    if (PopInputEmptyBuffer(&bufferMsg) == false)
    {
      retry++;
      continue;
    }

    for (i = 0; i < m_InputBufferCount; i++)
    {
      if (bufferMsg.pBuffer == m_InputBufferHeaders[i])
      {
        //readFrameLen = 0;
        if (m_InputDataSize == 0) {
          gettimeofday(&m_DecodeStartTime, NULL);
        }
        m_InputBufferHeaders[i]->nInputPortIndex = 0;
        m_InputBufferHeaders[i]->nOffset = 0;
        if ((readFrameLen = ReadBuffer(m_InputBufferHeaders[i])) <= 0)
        {
          VLOGD("NO FRAME READ");
          m_InputThreadRunning = false;
          break;
        }
        m_InputBufferHeaders[i]->nFilledLen = readFrameLen;
        m_InputBufferHeaders[i]->nInputPortIndex = PORT_INDEX_IN;
        m_InputBufferHeaders[i]->nFlags = 0;

        // FPS control process
        if (m_FpsControl && 0 != m_InputFrameNum)
        {
          if (0 != m_DecodeDuration)
          {
            struct timeval now;
            gettimeofday(&now, NULL);
            struct timeval frame_start_decode_time =
              m_DecodeFrameStartTime[(m_InputFrameNum - 1) % OMX_BUFFERS_NUM];
            int64_t diff = (now.tv_sec - frame_start_decode_time.tv_sec) * 1000 * 1000 + \
              (now.tv_usec - frame_start_decode_time.tv_usec);
            if (diff < m_DecodeDuration)
            {
              usleep(m_DecodeDuration - diff);
            }
          }
        }

        if (bWriteTimestamp)
        {
          nTimeStamp = nTimeStamp + (OMX_TICKS)(1000000 / settings->nFrameRate);
          m_InputBufferHeaders[i]->nTimeStamp = nTimeStamp;
        }

        gettimeofday(&m_DecodeFrameStartTime[m_InputFrameNum % OMX_BUFFERS_NUM], NULL);
        result = OMX_EmptyThisBuffer(m_Handle, m_InputBufferHeaders[i]);
        // if failed, then thread exit
        if ((result != OMX_ErrorNone) && (result != OMX_ErrorNoMore))
        {
          VLOGE("request empty buffer failed, exit");
          NotifyToStop();
          break;
        }
        m_InputFrameNum++;
        m_InputDataSize += readFrameLen;
        VLOGD("start decoding frame: %d \n\n", m_InputFrameNum);
      }
    }
  }

  VLOGD("input flag: %d, input %d buffers, retry: %d",
      m_InputThreadRunning, m_InputFrameNum, retry);
  FUNCTION_EXIT();

  return 0;
}

static void * OutputBufferProcessThread(void *arg)
{
  bool ret = false;
  OMX_ERRORTYPE result = OMX_ErrorUndefined;
  int32_t retry = 0;
  int32_t i = 0;
  struct timeval startWait;
  struct timeval endWait;

  FUNCTION_ENTER();

  m_OutputThreadRunning = true;
  gettimeofday(&startWait, NULL);
  while (retry < MAX_WAIT_TIMES)
  {
    if (waitForFirstTimeReconfigEvent)
    {

      pthread_mutex_lock(&m_ReconfigMutex);
      if (!m_NeedReconfigPort)
      {
        pthread_mutex_unlock(&m_ReconfigMutex);

        gettimeofday(&endWait, NULL);
        int diff = endWait.tv_sec - startWait.tv_sec;
        if (diff > MAX_WAIT_RECONFIG_DURATION)
        {
          waitForFirstTimeReconfigEvent = false;
          NotifyEventComplete();
        }
        continue;
      }
      if (m_NeedReconfigPort)
      {
        pthread_mutex_unlock(&m_ReconfigMutex);
        if (m_FreedOutputBufferCnt == m_OutputBufferCount)
        {
          BufferMessage_t bufferMsgToFree;
          while (PopOutputFilledBuffer(&bufferMsgToFree))
          {
            bufferMsgToFree.pBuffer = NULL;
          }
          ReleaseBuffers(PORT_INDEX_OUT);
          pthread_mutex_lock(&m_ReconfigMutex);
          pthread_cond_wait(&m_ReconfigSignal, &m_ReconfigMutex);
          pthread_mutex_unlock(&m_ReconfigMutex);
          waitForFirstTimeReconfigEvent = false;
        }
        continue;
      }
      pthread_mutex_unlock(&m_ReconfigMutex);
    }

    BufferMessage_t bufferMsg;
    if (PopOutputFilledBuffer(&bufferMsg) == false)
    {
      retry++;
      continue;
    }

    retry = 0;
    for (i = 0; i < m_OutputBufferCount; i++)
    {
      if (bufferMsg.pBuffer == m_OutputBufferHeaders[i])
      {
        VLOGD("Handling No.%d buffer now", i);
        break;
      }
    }

    if (i >= m_OutputBufferCount)
    {
      VLOGD("recv invalid buffer: 0x%p, len: %d",
          bufferMsg.pBuffer->pBuffer,
          bufferMsg.pBuffer->nFilledLen);
      continue;
    }

    // TODO: debug
    if (bufferMsg.pBuffer->nFilledLen == 0)
    {
      if (!m_NeedReconfigPort)
      {
        result = OMX_FillThisBuffer(m_Handle, bufferMsg.pBuffer);
        m_FreedOutputBufferCnt--;
        VLOGD("skip len == 0 buffer");
      }
      else
      {
        VLOGD("Filled length is 0, skip since during port reconfig");
      }
      continue;
    }

    m_OutputFrameNum++;
    VLOGD("recv decoded frame: %d, len: %d, ts: %lld, flag: %#X, buffer index:%d\n\n",
        m_OutputFrameNum, bufferMsg.pBuffer->nFilledLen, 
        bufferMsg.pBuffer->nTimeStamp, bufferMsg.pBuffer->nFlags, i);

    if (bufferMsg.pBuffer->nFilledLen > 0) {
      gettimeofday(&m_DecodeEndTime, NULL);
    }

    ret = StoreDecodedData(bufferMsg.pBuffer->pBuffer,
        bufferMsg.pBuffer->nFilledLen);
    if (ret == false)
    {
      VLOGE("Store decode data:%d, size:%d failed, exit",
          m_OutputFrameNum, bufferMsg.pBuffer->nFilledLen);
      m_OutputThreadRunning = false;
      NotifyToStop();
      break;
    }

    result = OMX_FillThisBuffer(m_Handle, bufferMsg.pBuffer);
    if ((result != OMX_ErrorNone) && (result != OMX_ErrorNoMore))
    {
      VLOGE("request fill buffer failed, exit");
      m_OutputThreadRunning = false;
      NotifyToStop();
      break;
    }
    m_FreedOutputBufferCnt--;
  }

  VLOGD("output flag: %d, frame num: %d, retry: %d",
      m_OutputThreadRunning, m_OutputFrameNum, retry);
  NotifyToStop();
  FUNCTION_EXIT();

  return 0;
}

bool StartProcessThread()
{
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

bool StartDecoder()
{
  bool ret = false;
  OMX_ERRORTYPE result = OMX_ErrorNone;
  int32_t i = 0;

  FUNCTION_ENTER();
  CHECK_HANDLE(m_Handle);

  ret = SetState(OMX_StateExecuting, OMX_TRUE);
  CHECK_BOOL("set state to executing failed", ret);

  for (i = 0; i < m_InputBufferCount; i++)
  {
    BufferMessage_t bufferMsg;
    bufferMsg.pBuffer = m_InputBufferHeaders[i];
    PushInputEmptyBuffer(bufferMsg);
  }

  // call fill this buffer
  for (i = 0; i < m_OutputBufferCount; i++)
  {
    m_OutputBufferHeaders[i]->nOutputPortIndex = 1;
    m_OutputBufferHeaders[i]->nFlags &= ~OMX_BUFFERFLAG_EOS;
    result = OMX_FillThisBuffer(m_Handle, m_OutputBufferHeaders[i]);
    CHECK_RESULT("request fill this buffer failed", result);
    m_FreedOutputBufferCnt--;
  }

  StartProcessThread();
  if (waitForFirstTimeReconfigEvent == 1)
  {
    WaitEventComplete();
    if (m_NeedReconfigPort)
    {
      bool res = ReconfigOutputPort();
      CHECK_BOOL("reconfig failed", res);
    }
  }

  FUNCTION_EXIT();
  return ret;
}

#ifdef _DEBUG_FRAMEPACK_
void PrintFramePackArrangement(OMX_QCOM_FRAME_PACK_ARRANGEMENT framePackingArrangement)
{
  VLOGD("id (%d)",
      framePackingArrangement.id);
  VLOGD("cancel_flag (%d)",
      framePackingArrangement.cancel_flag);
  VLOGD("type (%d)",
      framePackingArrangement.type);
  VLOGD("quincunx_sampling_flag (%d)",
      framePackingArrangement.quincunx_sampling_flag);
  VLOGD("content_interpretation_type (%d)",
      framePackingArrangement.content_interpretation_type);
  VLOGD("spatial_flipping_flag (%d)",
      framePackingArrangement.spatial_flipping_flag);
  VLOGD("frame0_flipped_flag (%d)",
      framePackingArrangement.frame0_flipped_flag);
  VLOGD("field_views_flag (%d)",
      framePackingArrangement.field_views_flag);
  VLOGD("current_frame_is_frame0_flag (%d)",
      framePackingArrangement.current_frame_is_frame0_flag);
  VLOGD("frame0_self_contained_flag (%d)",
      framePackingArrangement.frame0_self_contained_flag);
  VLOGD("frame1_self_contained_flag (%d)",
      framePackingArrangement.frame1_self_contained_flag);
  VLOGD("frame0_grid_position_x (%d)",
      framePackingArrangement.frame0_grid_position_x);
  VLOGD("frame0_grid_position_y (%d)",
      framePackingArrangement.frame0_grid_position_y);
  VLOGD("frame1_grid_position_x (%d)",
      framePackingArrangement.frame1_grid_position_x);
  VLOGD("frame1_grid_position_y (%d)",
      framePackingArrangement.frame1_grid_position_y);
  VLOGD("reserved_byte (%d)",
      framePackingArrangement.reserved_byte);
  VLOGD("repetition_period (%d)",
      framePackingArrangement.repetition_period);
  VLOGD("extension_flag (%d)",
      framePackingArrangement.extension_flag);
}
#endif //_DEBUG_FRAMEPACK_

void WaitingForTestDone()
{
  FUNCTION_ENTER();
  bool result = true;

  pthread_mutex_lock(&m_RunningMutex);

  while (m_Running == true && result)
  {
    pthread_cond_wait(&m_RunningSignal, &m_RunningMutex);
    if (m_NeedReconfigPort)
    {
      pthread_mutex_unlock(&m_RunningMutex);
      //result = ReconfigOutputPort();
      // TODO handle reconfig during decoding
      pthread_mutex_lock(&m_RunningMutex);
    }
  }

  pthread_mutex_unlock(&m_RunningMutex);
  FUNCTION_EXIT();
}

bool ReconfigOutputPort()
{
  OMX_ERRORTYPE result = OMX_ErrorNone;
  FUNCTION_ENTER();
  if (!DisableOutputPort())
  {
    VLOGE("Failed to disable output port.");
    FUNCTION_EXIT();
    return false;
  }

  OMX_PARAM_PORTDEFINITIONTYPE portdef;
  OMX_INIT_STRUCT_SIZE(&portdef, OMX_PARAM_PORTDEFINITIONTYPE);
  portdef.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  result = OMX_GetParameter(m_Handle, OMX_IndexParamPortDefinition, &portdef);
  CHECK_RESULT("get OMX_IndexParamPortDefinition error", result);
  VLOGD("Decoder: Output min buffer count %u", (unsigned int)portdef.nBufferCountMin);
  VLOGD("Decoder: Output buffer size %u", (unsigned int)portdef.nBufferSize);
  VLOGD("Decoder: Output actual output buffer count %u", (unsigned int)portdef.nBufferCountActual);
  VLOGD("Decoder: Output WxH changed to %dx%d", portdef.format.video.nFrameWidth, portdef.format.video.nFrameHeight);

  if (OMX_DirOutput != portdef.eDir)
  {
    VLOGE("Error: Expect output port");
    FUNCTION_EXIT();
    return false;
  }

  settings->nFrameHeight = portdef.format.video.nFrameHeight;
  crop_rect.nHeight = portdef.format.video.nFrameHeight;
  settings->nFrameWidth = portdef.format.video.nFrameWidth;
  crop_rect.nWidth = portdef.format.video.nFrameWidth;
  VLOGD("Reconfig the WxH as %dx%d", settings->nFrameWidth, settings->nFrameHeight);

  // We don't handle eColorFormat changing to UBWC of portdef even for TP10_UBWC,
  // since the setting can not work under the situation, OMX_ErrorUnsupportedSetting will occur.
  // set back the gotten settings directly.
  result = OMX_SetParameter(m_Handle, OMX_IndexParamPortDefinition, &portdef);
  CHECK_RESULT("Output reconfig re-set OMX_IndexParamPortDefinition error", result);

  if (!EnableOutputPort())
  {
    VLOGE("Failed to enable output port.");
    FUNCTION_EXIT();
    return false;
  }

  FUNCTION_EXIT();
  return true;
}

bool DisableOutputPort()
{
  FUNCTION_ENTER();

  pthread_mutex_lock(&m_ReconfigMutex);
  //m_NeedReconfigPort = true;
  // Send DISABLE command
  OMX_ERRORTYPE result = OMX_SendCommand(m_Handle, OMX_CommandPortDisable, PORT_INDEX_OUT, 0);
  pthread_mutex_unlock(&m_ReconfigMutex);
  if (OMX_ErrorNone != result)
  {
    VLOGE("Disable output port failed.");
    FUNCTION_EXIT();
    return false;
  }
  // wait for Disable event to come back
  WaitEventComplete();

#if 0 //We didn't use PMEM info, so don't need this part as for now
  if(p_eglHeaders) {
    free(p_eglHeaders);
    p_eglHeaders = NULL;
  }
  if (pPMEMInfo)
  {
    DEBUG_PRINT("Freeing in external pmem case:PMEM");
    free(pPMEMInfo);
    pPMEMInfo = NULL;
  }
  if (pPlatformEntry)
  {
    DEBUG_PRINT("Freeing in external pmem case:ENTRY");
    free(pPlatformEntry);
    pPlatformEntry = NULL;
  }
  if (pPlatformList)
  {
    DEBUG_PRINT("Freeing in external pmem case:LIST");
    free(pPlatformList);
    pPlatformList = NULL;
  }
  if (currentStatus == ERROR_STATE)
  {
    do_freeHandle_and_clean_up(true);
  FUNCTION_EXIT();
    return -1;
  }
#endif

  FUNCTION_EXIT();
  return true;
}

bool EnableOutputPort()
{
  int bufCnt = 0;
  OMX_ERRORTYPE result = OMX_ErrorNone;
  FUNCTION_ENTER();
  // Send Enable command
  result = OMX_SendCommand(m_Handle, OMX_CommandPortEnable, PORT_INDEX_OUT, 0);
  if (OMX_ErrorNone != result)
  {
    VLOGE("Enable output port failed.");
    FUNCTION_EXIT();
    return false;
  }

  OMX_PARAM_PORTDEFINITIONTYPE portdef;
  OMX_INIT_STRUCT_SIZE(&portdef, OMX_PARAM_PORTDEFINITIONTYPE);
  portdef.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  result = OMX_GetParameter(m_Handle, OMX_IndexParamPortDefinition, &portdef);
  CHECK_RESULT("get OMX_IndexParamPortDefinition error", result);
  VLOGD("Check o/p port buffer cnt and size:   org cnt:%d,   size:%d,   new cnt:%d,   size:%d",
      m_OutputBufferCount, m_OutputBufferSize, portdef.nBufferCountActual, portdef.nBufferSize);
  // we ignore anti-flickering check as for now.
  m_OutputBufferSize = portdef.nBufferSize;
  m_OutputBufferCount = portdef.nBufferCountActual;
  m_FreedOutputBufferCnt = m_OutputBufferCount;

  for (int i = 0; i < m_OutputBufferCount; i++)
  {
    result = OMX_AllocateBuffer(m_Handle,
        &m_OutputBufferHeaders[i],
        (OMX_U32) PORT_INDEX_OUT,
        NULL,
        m_OutputBufferSize);
    CHECK_RESULT("Allocate output buffer failed", result);
  }

  // wait for enable event to come back
  WaitEventComplete();
  for(bufCnt = 0; bufCnt < m_OutputBufferCount; bufCnt++)
  {
    if (m_OutputBufferHeaders[bufCnt] == NULL)
    {
      VLOGE("m_OutputBufferHeaders[%d] is NULL", bufCnt);
      FUNCTION_EXIT();
      return false;
    }

    m_OutputBufferHeaders[bufCnt]->nOutputPortIndex = PORT_INDEX_OUT;
    m_OutputBufferHeaders[bufCnt]->nFlags &= ~OMX_BUFFERFLAG_EOS;
    result = OMX_FillThisBuffer(m_Handle, m_OutputBufferHeaders[bufCnt]);
    CHECK_RESULT("request fill this buffer failed", result);
  }
  FUNCTION_EXIT();
  return true;
}

void WaitProcessThreadExit()
{
  FUNCTION_ENTER();
  pthread_join(m_InputProcessThreadId, NULL);
  pthread_join(m_OutPutProcessThreadId, NULL);
  FUNCTION_EXIT();
}

bool StopDecoder()
{
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

bool ReleaseBuffers(PortIndexType port)
{
  OMX_ERRORTYPE result = OMX_ErrorNone;
  int32_t i = 0;

  FUNCTION_ENTER();
  CHECK_HANDLE(m_Handle);

  VLOGD("release input buffer");
  if (PORT_INDEX_ALL == port || PORT_INDEX_IN == port)
  {
    // release InputBuffer
    for (i = 0; i < m_InputBufferCount; i++)
    {
      if (m_InputBufferHeaders[i] != NULL)
      {
        if (!m_IsSWCodec)
        {
          if (m_InputBufferHeaders[i]->pBuffer)
          {
            free(m_InputBufferHeaders[i]->pBuffer);
          }
        }
        result = OMX_FreeBuffer(m_Handle,
            PORT_INDEX_IN,
            m_InputBufferHeaders[i]);
        CHECK_RESULT("free input buffer header failed", result);
      }
    }
  }
  VLOGD("release output buffer");
  if (PORT_INDEX_ALL == port || PORT_INDEX_OUT == port)
  {
    // release OutputBuffer
    for (i = 0; i < m_OutputBufferCount; i++)
    {
      if (m_OutputBufferHeaders[i] != NULL)
      {
       VLOGD("release m_OutputBufferHeaders:%d", i);
       /* freeVideoBuffer((OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO*)m_OutputBufferHeaders[i]->pAppPrivate,
            m_OutputBufferHeaders[i]->pBuffer,
            m_OutputBufferSize, &mIonDataArray[i]);
      }
      delete (OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO*) m_OutputBufferHeaders[i]->pAppPrivate;*/
 
        result = OMX_FreeBuffer(m_Handle,
            PORT_INDEX_OUT,
            m_OutputBufferHeaders[i]);

        CHECK_RESULT("free output buffer header failed", result);
      }
    }

    /*if (mIonDataArray)
    {
      free(mIonDataArray);
    }*/
  }

  FUNCTION_EXIT();
  return true;
}

bool ReleaseCodec()
{
  FUNCTION_ENTER();
  CHECK_HANDLE(m_Handle);

  bool ret = SetState(OMX_StateLoaded, OMX_FALSE);
  CHECK_BOOL("set state to loaded failed", ret);

  ReleaseBuffers(PORT_INDEX_ALL);

  ret = WaitState(OMX_StateLoaded);
  CHECK_BOOL("wait state to loaded failed", ret);

  if (m_Handle != NULL)
  {
    OMX_FreeHandle(m_Handle);
    m_Handle = NULL;
  }

  VLOGD("handle : %p", &m_Handle);
  VLOGD("Decode input frame: %d, output frame: %d",
      m_InputFrameNum, m_OutputFrameNum);

  FUNCTION_EXIT();
  return true;
}

void ReleaseResource()
{
  FUNCTION_ENTER();

  ReleaseCodec();
  if (m_InputFileFd >= 0)
  {
    close(m_InputFileFd);
    m_InputFileFd = -1;
  }

  if (m_OutputFileFd >= 0)
  {
    close(m_OutputFileFd);
    m_OutputFileFd = -1;
  }
  DestroyQueue();
  pthread_mutex_destroy(&m_RunningMutex);
  pthread_mutex_destroy(&m_EventCompleteMutex);
  pthread_mutex_destroy(&m_ReconfigMutex);
  pthread_cond_destroy(&m_ReconfigSignal);
  pthread_cond_destroy(&m_RunningSignal);
  pthread_cond_destroy(&m_EventCompleteSignal);

  FUNCTION_EXIT();
}

