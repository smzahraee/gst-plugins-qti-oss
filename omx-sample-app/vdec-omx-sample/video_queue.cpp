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

#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "OMX_Core.h"

// log control
extern uint32_t debug_level_sets;
#include "video_test_debug.h"
#include "video_queue.h"

#define MAX_QUEUE_SIZE 100
#define MAX_TIMEOUT_SECOND 1   // 1s

// used by OMX component state transfer
typedef struct StateMessageQueue
{
  StateEvent_t states[MAX_QUEUE_SIZE];
  uint32_t head;
  uint32_t tail;
  uint32_t size;
} StateMessageQueue_t;

// used by OMX component buffer manage
typedef struct BufferMessageQueue
{
  BufferMessage_t buffers[MAX_QUEUE_SIZE];
  uint32_t head;
  uint32_t tail;
  uint32_t size;
} BufferMessageQueue_t;

static StateMessageQueue_t m_StateQueue;
static pthread_mutex_t m_StateMutex;
static pthread_cond_t m_StateSignal;

static BufferMessageQueue_t m_InputEmptyBufferQueue;
static pthread_mutex_t m_InputMutex;
static pthread_cond_t m_InputSignal;

static BufferMessageQueue_t m_OutputFilledBufferQueue;
static pthread_mutex_t m_OutputMutex;
static pthread_cond_t m_OutputSignal;


void InitQueue()
{
  FUNCTION_ENTER();

  // init for state event queue
  pthread_mutex_init(&m_StateMutex, NULL);
  pthread_cond_init(&m_StateSignal, NULL);
  memset(&m_StateQueue, 0, sizeof(m_StateQueue));

  // init for input buffer queue
  pthread_mutex_init(&m_InputMutex, NULL);
  pthread_cond_init(&m_InputSignal, NULL);
  memset(&m_InputEmptyBufferQueue, 0, sizeof(m_InputEmptyBufferQueue));

  // init for output buffer queue
  pthread_mutex_init(&m_OutputMutex, NULL);
  pthread_cond_init(&m_OutputSignal, NULL);
  memset(&m_OutputFilledBufferQueue, 0, sizeof(m_OutputFilledBufferQueue));

  FUNCTION_EXIT();
}

void DestroyQueue()
{
  FUNCTION_ENTER();

  // for state event queue
  pthread_mutex_destroy(&m_StateMutex);
  pthread_cond_destroy(&m_StateSignal);

  // for state event queue
  pthread_mutex_destroy(&m_InputMutex);
  pthread_cond_destroy(&m_InputSignal);

  // for state event queue
  pthread_mutex_destroy(&m_OutputMutex);
  pthread_cond_destroy(&m_OutputSignal);

  FUNCTION_EXIT();
}

// for OMX state event
bool PushEvent(StateEvent_t newState)
{
  FUNCTION_ENTER();

  VLOGD("size:%d, head:%d, tail:%d",
      m_StateQueue.size,
      m_StateQueue.head,
      m_StateQueue.tail);

  pthread_mutex_lock(&m_StateMutex);
  if (m_StateQueue.size >= MAX_QUEUE_SIZE)
  {
    VLOGE("State event queue is full");
    pthread_mutex_unlock(&m_StateMutex);
    return false;
  }

  m_StateQueue.states[m_StateQueue.head] = newState;
  m_StateQueue.head = (m_StateQueue.head + 1) % MAX_QUEUE_SIZE;
  m_StateQueue.size++;
  pthread_cond_signal(&m_StateSignal);
  pthread_mutex_unlock(&m_StateMutex);

  VLOGD("size:%d, head:%d, tail:%d",
      m_StateQueue.size,
      m_StateQueue.head,
      m_StateQueue.tail);

  FUNCTION_EXIT();
  return true;
}

bool PopEvent(StateEvent_t* pEvent)
{
  FUNCTION_ENTER();

  if (pEvent == NULL)
  {
    VLOGE("Invalid paramter!");
    FUNCTION_EXIT();
    return false;
  }

  VLOGD("size:%d, head:%d, tail:%d",
      m_StateQueue.size,
      m_StateQueue.head,
      m_StateQueue.tail);

  pthread_mutex_lock(&m_StateMutex);

  if (m_StateQueue.size == 0)
  {
    struct timeval now;
    gettimeofday(&now, NULL);

    struct timespec outtime;
    outtime.tv_sec = now.tv_sec + MAX_TIMEOUT_SECOND;  // timeout: 1s
    outtime.tv_nsec = now.tv_usec * 1000;
    int ret = pthread_cond_timedwait(&m_StateSignal,
        &m_StateMutex, &outtime);
    if (ret == ETIMEDOUT)
    {
      pthread_mutex_unlock(&m_StateMutex);
      VLOGE("Wait signal timeout");
      FUNCTION_EXIT();
      return false;
    }
    else if (ret < 0)
    {
      VLOGE("Wait signal error: %d", ret);
      FUNCTION_EXIT();
      return false;
    }
  }
  *pEvent = m_StateQueue.states[m_StateQueue.tail];
  m_StateQueue.tail = (m_StateQueue.tail + 1) % MAX_QUEUE_SIZE;
  --m_StateQueue.size;
  pthread_mutex_unlock(&m_StateMutex);

  VLOGD("size:%d, head:%d, tail:%d",
      m_StateQueue.size,
      m_StateQueue.head,
      m_StateQueue.tail);

  FUNCTION_EXIT();
  return true;
}

// for OMX input buffer
bool PushInputEmptyBuffer(BufferMessage_t buffer)
{
  FUNCTION_ENTER();

  VLOGD("size:%d, head:%d, tail:%d",
      m_InputEmptyBufferQueue.size,
      m_InputEmptyBufferQueue.head,
      m_InputEmptyBufferQueue.tail);

  pthread_mutex_lock(&m_InputMutex);
  if (m_InputEmptyBufferQueue.size >= MAX_QUEUE_SIZE)
  {
    VLOGE("State event queue is full");
    pthread_mutex_unlock(&m_InputMutex);
    FUNCTION_EXIT();
    return false;
  }

  m_InputEmptyBufferQueue.buffers[m_InputEmptyBufferQueue.head] = buffer;
  m_InputEmptyBufferQueue.head = (m_InputEmptyBufferQueue.head + 1) % MAX_QUEUE_SIZE;
  m_InputEmptyBufferQueue.size++;
  pthread_cond_signal(&m_InputSignal);
  pthread_mutex_unlock(&m_InputMutex);

  VLOGD("size:%d, head:%d, tail:%d",
      m_InputEmptyBufferQueue.size,
      m_InputEmptyBufferQueue.head,
      m_InputEmptyBufferQueue.tail);
  FUNCTION_EXIT();
  return true;
}

bool PopInputEmptyBuffer(BufferMessage_t* pBuffer)
{
  FUNCTION_ENTER();

  if (pBuffer == NULL) {
    VLOGE("Invalid paramter!");
    return false;
  }

  VLOGD("size:%d, head:%d, tail:%d",
      m_InputEmptyBufferQueue.size,
      m_InputEmptyBufferQueue.head,
      m_InputEmptyBufferQueue.tail);
  pthread_mutex_lock(&m_InputMutex);
  if (m_InputEmptyBufferQueue.size == 0)
  {
    struct timeval now;
    gettimeofday(&now, NULL);

    struct timespec outtime;
    outtime.tv_sec = now.tv_sec + MAX_TIMEOUT_SECOND;  // timeout: 1s
    outtime.tv_nsec = now.tv_usec * 1000;

    int ret = pthread_cond_timedwait(&m_InputSignal,
        &m_InputMutex, &outtime);
    if (ret == ETIMEDOUT)
    {
      pthread_mutex_unlock(&m_InputMutex);
      VLOGE("timeout");
      FUNCTION_EXIT();
      return false;
    }
  }
  *pBuffer = m_InputEmptyBufferQueue.buffers[m_InputEmptyBufferQueue.tail];
  m_InputEmptyBufferQueue.tail = (m_InputEmptyBufferQueue.tail + 1) % MAX_QUEUE_SIZE;
  --m_InputEmptyBufferQueue.size;
  pthread_mutex_unlock(&m_InputMutex);

  VLOGD("size:%d, head:%d, tail:%d",
      m_InputEmptyBufferQueue.size,
      m_InputEmptyBufferQueue.head,
      m_InputEmptyBufferQueue.tail);

  FUNCTION_EXIT();
  return true;
}

// for OMX output buffer
bool PushOutputFilledBuffer(BufferMessage_t buffer)
{
  FUNCTION_ENTER();

  VLOGD("size:%d, head:%d, tail:%d",
      m_OutputFilledBufferQueue.size,
      m_OutputFilledBufferQueue.head,
      m_OutputFilledBufferQueue.tail);

  pthread_mutex_lock(&m_OutputMutex);
  if (m_OutputFilledBufferQueue.size >= MAX_QUEUE_SIZE)
  {
    VLOGE("State event queue is full");
    pthread_mutex_unlock(&m_OutputMutex);
    FUNCTION_EXIT();
    return false;
  }
  m_OutputFilledBufferQueue.buffers[m_OutputFilledBufferQueue.head] = buffer;
  m_OutputFilledBufferQueue.head = (m_OutputFilledBufferQueue.head + 1) % MAX_QUEUE_SIZE;
  m_OutputFilledBufferQueue.size++;
  pthread_cond_signal(&m_OutputSignal);
  pthread_mutex_unlock(&m_OutputMutex);

  VLOGD("size:%d, head:%d, tail:%d",
      m_OutputFilledBufferQueue.size,
      m_OutputFilledBufferQueue.head,
      m_OutputFilledBufferQueue.tail);

  FUNCTION_EXIT();
  return true;
}

bool PopOutputFilledBuffer(BufferMessage_t* pBuffer)
{
  FUNCTION_ENTER();

  if (pBuffer == NULL)
  {
    VLOGE("Invalid paramter!");
    return false;
  }

  VLOGD("size:%d, head:%d, tail:%d",
      m_OutputFilledBufferQueue.size,
      m_OutputFilledBufferQueue.head,
      m_OutputFilledBufferQueue.tail);

  pthread_mutex_lock(&m_OutputMutex);
  if (m_OutputFilledBufferQueue.size == 0)
  {
    struct timeval now;
    gettimeofday(&now, NULL);

    struct timespec outtime;
    outtime.tv_sec = now.tv_sec + MAX_TIMEOUT_SECOND;  // timeout: 1s
    outtime.tv_nsec = now.tv_usec * 1000;

    int ret = pthread_cond_timedwait(&m_OutputSignal,
        &m_OutputMutex, &outtime);
    if (ret == ETIMEDOUT)
    {
      pthread_mutex_unlock(&m_OutputMutex);
      VLOGE("timeout");
      FUNCTION_EXIT();
      return false;
    }
  }
  *pBuffer = m_OutputFilledBufferQueue.buffers[m_OutputFilledBufferQueue.tail];
  m_OutputFilledBufferQueue.tail = (m_OutputFilledBufferQueue.tail + 1) % MAX_QUEUE_SIZE;
  --m_OutputFilledBufferQueue.size;
  pthread_mutex_unlock(&m_OutputMutex);

  VLOGD("size:%d, head:%d, tail:%d",
      m_OutputFilledBufferQueue.size,
      m_OutputFilledBufferQueue.head,
      m_OutputFilledBufferQueue.tail);

  FUNCTION_EXIT();
  return true;
}
