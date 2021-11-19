/*-------------------------------------------------------------------
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
--------------------------------------------------------------------*/

#ifndef _CRYPTO_H
#define _CRYPTO_H

#include <linux/dma-buf.h>

#include "OMX_Core.h"
#include <gst/gst.h>

struct secure_handle {
  unsigned char *ion_sbuffer;
};

typedef unsigned int uint32;

//Errors
typedef enum SecureCopyResult {
  SECURE_COPY_SUCCESS                           = 0,
  SECURE_COPY_ERROR_COPY_FAILED                 = 1,
  SECURE_COPY_ERROR_INIT_FAILED                 = 2,
  SECURE_COPY_ERROR_TERMINATE_FAILED            = 3,
  SECURE_COPY_ERROR_ION_MALLOC_FAILED           = 4,
  SECURE_COPY_ERROR_ION_FREE_FAILED             = 5,
  SECURE_COPY_ERROR_NSS_COPY_FAILED             = 6,
  SECURE_COPY_ERROR_SNS_COPY_FAILED             = 7,
  SECURE_COPY_ERROR_MEM_SEG_COPY_FAILED         = 8,
  SECURE_COPY_ERROR_INVALID_PARAMS              = 9,
  SECURE_COPY_ERROR_FEATURE_NOT_SUPPORT         = 10,
  SECURE_COPY_ERROR_BUFFER_TOO_SHORT            = 11,
  SECURE_COPY_ERROR_SECURE_ION_MALLOC_FAILED    = 12,
  SECURE_COPY_ERROR_FEATURE_NOT_SUPPORTED       = 13,
  SECURE_COPY_FAILURE = 0x7FFFFFFF
} SecureCopyResult;

typedef enum SecureCopyDir {
  SECURE_COPY_NONSECURE_TO_SECURE = 0,
  SECURE_COPY_SECURE_TO_NONSECURE,
  SECURE_COPY_INVALID_DIR
} SecureCopyDir;


typedef SecureCopyResult(*Crypto_Set_AppName)(const char *name);
typedef SecureCopyResult(*Crypto_Init)(struct secure_handle **);
typedef SecureCopyResult(*Crypto_Deinit)(struct secure_handle **);
typedef SecureCopyResult(*Crypto_Copy)(struct secure_handle *,
        OMX_U8 *, const uint32, uint32, uint32, uint32 *, SecureCopyDir);

typedef struct secure_handle secure_handle;

typedef struct Crypto {
    void *m_lib_handle;
    secure_handle *m_secure_handle;
    Crypto_Init m_crypto_init;
    Crypto_Set_AppName m_crypto_set_appname;
    Crypto_Deinit m_crypto_deinit;
    Crypto_Copy m_crypto_copy;
} Crypto;

OMX_ERRORTYPE crypto_init(Crypto *crypto);
OMX_ERRORTYPE crypto_terminate(Crypto *crypto);
OMX_ERRORTYPE crypto_copy(Crypto *crypto, SecureCopyDir eCopyDir,
    OMX_U8 *pBuffer, unsigned long nBufferFd, OMX_U32 nBufferSize);
OMX_ERRORTYPE load_crypto_lib(Crypto *crypto);
void unload_crypto_lib(Crypto *crypto);



#endif //#ifndef _CRYPTO_H
