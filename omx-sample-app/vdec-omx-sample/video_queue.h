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

#ifndef VDEC_OMX_SAMPLE_VIDEO_QUEUE_H_
#define VDEC_OMX_SAMPLE_VIDEO_QUEUE_H_

typedef struct StateEvent
{
  uint32_t event;
  uint32_t cmd;
  uint32_t cmdData;
  uint32_t cmdResult;
} StateEvent_t;

typedef struct BufferMessage
{
  OMX_BUFFERHEADERTYPE* pBuffer;
} BufferMessage_t;

// init all message queue
void InitQueue();
void DestroyQueue();

// event queue
bool PushEvent(StateEvent_t newState);
bool PopEvent(StateEvent_t* pEvent);

// input port buffer queue
bool PushInputEmptyBuffer(BufferMessage_t buffer);
bool PopInputEmptyBuffer(BufferMessage_t* pBuffer);

// output port buffer queue
bool PushOutputFilledBuffer(BufferMessage_t buffer);
bool PopOutputFilledBuffer(BufferMessage_t* pBuffer);

#endif  // VDEC_OMX_SAMPLE_VIDEO_QUEUE_H_
