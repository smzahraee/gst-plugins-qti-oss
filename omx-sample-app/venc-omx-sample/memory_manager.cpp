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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>

#include "OMX_QCOMExtns.h"

extern uint32_t m_DebugLevelSets;
extern char * m_MemoryMode;

#include "video_debug.h"
#include "memory_manager.h"

// int m_IonDeviceFd;
int m_DeviceFd = -1;
struct gbm_device *m_GbmDevice = NULL;

static const char* PMEM_DEVICE;

bool InitBufferManager() {
  FUNCTION_ENTER();
  VLOGD("mode: %s", m_MemoryMode);
  VLOGD("result: %d", strncasecmp(m_MemoryMode, "gbm", 3));

  if (!strncasecmp(m_MemoryMode, "gbm", 3)) {
    PMEM_DEVICE = "/dev/dri/renderD128";
    m_DeviceFd = open (PMEM_DEVICE, O_RDWR | O_CLOEXEC);

    if (m_DeviceFd < 0) {
      VLOGE("ERROR: gbm Device: %s open failed, errno:%d, err:%s",
          PMEM_DEVICE, errno, strerror(errno));
      FUNCTION_EXIT();
      return true;
    }
    m_GbmDevice = gbm_create_device(m_DeviceFd);
    if (m_GbmDevice == NULL)
    {
      close(m_DeviceFd);
      m_DeviceFd = -1;
      VLOGE("gbm_create_device failed\n");
    }
  } else if (!strncasecmp(m_MemoryMode, "ion", 3)) {
    PMEM_DEVICE = "/dev/ion";
    m_DeviceFd = ion_open();
    if(m_DeviceFd < 0)
    {
      VLOGE("ION Device open() Failed\n");
    }
  } else {
    m_DeviceFd = open(PMEM_DEVICE, O_RDWR);
    if (m_DeviceFd < 0)
    {
      VLOGE("device open failed\n");
    }
  }
  FUNCTION_EXIT();
  return true;
}

void ReleaseBufferManager() {
  FUNCTION_ENTER();
  if (!strncasecmp(m_MemoryMode, "gbm", 3)) {
    if (m_GbmDevice) {
      gbm_device_destroy(m_GbmDevice);
      m_GbmDevice = NULL;
    }
  }
  close(m_DeviceFd);
  m_DeviceFd =-1;
  FUNCTION_EXIT();
}

void* AllocVideoBuffer(OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO* p_mem, int size, struct EncodeIon *ion_data_ptr, int width, int height, uint32_t format, bool ubwc_flag)
{
  FUNCTION_ENTER();
  void *p_virt = NULL;
  int rc = 0;

  if (!p_mem || !ion_data_ptr)
  {
    return NULL;
  }

  if (!strncasecmp(m_MemoryMode, "gbm", 3)) {
    struct gbm_bo *bo = NULL;
    int bo_fd = -1, meta_fd = -1;
    int align_size = size;
    align_size = (align_size + 4096 - 1) & ~(4096 - 1);
    uint32_t flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;

    VLOGD("use gbm\n");
    ion_data_ptr->ion_device_fd = m_DeviceFd;
    if(ion_data_ptr->ion_device_fd < 0)
    {
      VLOGE("gbm Device open() Failed");
      goto error_handle;
    }
    ion_data_ptr->gbm = m_GbmDevice;
    if (ion_data_ptr->gbm == NULL)
    {
      VLOGE("gbm_create_device failed\n");
      goto error_handle;
    }
    if (ubwc_flag) {
      flags |= GBM_BO_USAGE_UBWC_ALIGNED_QTI;
    }
    bo = gbm_bo_create(ion_data_ptr->gbm, width, height, format, flags);
    if(bo == NULL) {
      VLOGE("Create bo failed \n");
      goto error_handle;
    }
    ion_data_ptr->bo = bo;
    bo_fd = gbm_bo_get_fd(bo);
    if(bo_fd < 0) {
      VLOGE("Get bo fd failed \n");
      goto error_handle;
    }
    ion_data_ptr->data_fd = bo_fd;
    gbm_perform(GBM_PERFORM_GET_METADATA_ION_FD, bo, &meta_fd);
    if(meta_fd < 0) {
      VLOGE("Get bo meta fd failed \n");
      goto error_handle;
    }
    ion_data_ptr->meta_fd = meta_fd;
    VLOGD("gbm buffer size: app calculate size %d, gbm internal calculate size %d\n", size, bo->size);
    if (size != bo->size) {
      VLOGE("!!!!! app calculated size isn't equal to gbm bo internal calculated size !!!!\n");
      goto error_handle;
    }
    p_mem->pmem_fd = ion_data_ptr->data_fd;
  } else if (!strncasecmp(m_MemoryMode, "ion", 3)) {
    ion_data_ptr->ion_device_fd = m_DeviceFd;
    if(ion_data_ptr->ion_device_fd < 0)
    {
      VLOGE("ION Device open() Failed");
      goto error_handle;
    }
    size = (size + 4095) & (~4095);
    ion_data_ptr->alloc_data.len = size;
    ion_data_ptr->alloc_data.heap_id_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);
    ion_data_ptr->alloc_data.flags = 0;

    rc = ion_alloc_fd(ion_data_ptr->ion_device_fd, ion_data_ptr->alloc_data.len, 0,
        ion_data_ptr->alloc_data.heap_id_mask, ion_data_ptr->alloc_data.flags,
        &ion_data_ptr->data_fd);
    p_mem->pmem_fd = ion_data_ptr->data_fd;
  } else {
    p_mem->pmem_fd = m_DeviceFd;
    if ((int)(p_mem->pmem_fd) < 0) {
      return NULL;
    }
    size = (size + 4095) & (~4095);
  }

  p_mem->offset = 0;
  p_virt = mmap(NULL, size,
      PROT_READ | PROT_WRITE,
      MAP_SHARED, p_mem->pmem_fd, p_mem->offset);
  if (p_virt == (void*) MAP_FAILED)
  {
    goto error_handle;
  }

  VLOGD("allocated pMem->fd = %lu pVirt=%p, pMem->phys(offset)=0x%lx, size = %d", p_mem->pmem_fd,
      p_virt, p_mem->offset, size);

  //Clean total frame memory content. For some non-MB-aligned encoding, like 1920x1080,
  //the extra 1081~1088 line's content still probably affect encoded result if VPU don't do special operation for those extra line.
  //Therefore, it's better to clean those extra lines in advance.

  VLOGD("Clean frame buffer's total content (size %d) as 0\n", size);
  memset(p_virt, 0, size);

  FUNCTION_EXIT();
  return p_virt;

error_handle:
  if (!strncasecmp(m_MemoryMode, "gbm", 3)) {
    if (ion_data_ptr->bo) {
      gbm_bo_destroy(ion_data_ptr->bo);
    }
    ion_data_ptr->bo = NULL;
    ion_data_ptr->meta_fd = -1;
  } else if (!strncasecmp(m_MemoryMode, "ion", 3)) {
    close(ion_data_ptr->data_fd);
    ion_data_ptr->data_fd =-1;
  }
  FUNCTION_EXIT();
  return NULL;
}

int FreeVideoBuffer(OMX_QCOM_PLATFORM_PRIVATE_PMEM_INFO* p_mem, void* p_virt, int size, struct EncodeIon *ion_data_ptr) {
  FUNCTION_ENTER();
  if (!p_mem || !p_virt || !ion_data_ptr)
    return -1;

  size = (size + 4095) & (~4095);
  munmap(p_virt, size);

  if (!strncasecmp(m_MemoryMode, "gbm", 3)) {
    if (ion_data_ptr->bo) {
      gbm_bo_destroy(ion_data_ptr->bo);
    }
    ion_data_ptr->bo = NULL;
    ion_data_ptr->meta_fd = -1;
  } else if (!strncasecmp(m_MemoryMode, "ion", 3)) {
    close(ion_data_ptr->data_fd);
    ion_data_ptr->data_fd =-1;
  }
  FUNCTION_EXIT();
  return 0;
}

