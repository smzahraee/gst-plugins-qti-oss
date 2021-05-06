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

#ifndef VENC_OMX_SAMPLE_MEMORY_MANAGER_H_
#define VENC_OMX_SAMPLE_MEMORY_MANAGER_H_

#include "gbm.h"
#include "gbm_priv.h"

#include <linux/msm_ion.h>
#if TARGET_ION_ABI_VERSION >= 2
#include <ion/ion.h>
#include <linux/dma-buf.h>
#else
#include <linux/ion.h>
#endif

#include "OMX_QCOMExtns.h"

struct EncodeIon
{
  int ion_device_fd;
  struct ion_allocation_data alloc_data;
  int data_fd;
  struct gbm_device *gbm;
  struct gbm_bo *bo;
  int meta_fd;
};

bool InitBufferManager();
void ReleaseBufferManager();
void* AllocVideoBuffer(OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO* pmem, int size, struct EncodeIon *ion_data_ptr, int width, int height, uint32_t format, bool ubwc_flag);
int FreeVideoBuffer(OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO* pmem, void* pvirt, int size, struct EncodeIon *ion_data_ptr);

#endif  // VENC_OMX_SAMPLE_MEMORY_MANAGER_H_
