/*-------------------------------------------------------------------
Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.

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

#include "crypto.h"
#include <dlfcn.h>

#define SymOEMCryptoLib "libcontentcopy.so"
#define SymOEMCryptoAppName "smpcpyap64"
#define SymOEMCryptoSetAppName "Content_Protection_Set_AppName"
#define SymOEMCryptoInit "Content_Protection_Copy_Init"
#define SymOEMCryptoTerminate "Content_Protection_Copy_Terminate"
#define SymOEMCryptoCopy "Content_Protection_Copy"

OMX_ERRORTYPE crypto_init(Crypto *crypto) {

    crypto->m_lib_handle = NULL;
    crypto->m_secure_handle = NULL;
    crypto->m_crypto_init = NULL;
    crypto->m_crypto_set_appname = NULL;
    crypto->m_crypto_deinit = NULL;
    crypto->m_crypto_copy = NULL;
    GST_DEBUG ("Crypto init");
    OMX_ERRORTYPE result = load_crypto_lib(crypto);
    if (result == OMX_ErrorNone) {
        if (crypto->m_crypto_init) {
            result = (OMX_ERRORTYPE)crypto->m_crypto_init(&crypto->m_secure_handle);
            if (crypto->m_crypto_set_appname) {
                result = (OMX_ERRORTYPE)crypto->m_crypto_set_appname(SymOEMCryptoAppName);
            } else {
                GST_ERROR("Invalid method handle to OEMCryptoSetAppName");
                result = OMX_ErrorBadParameter;
            }
        } else {
            GST_ERROR("Invalid method handle to OEMCryptoInit");
            result = OMX_ErrorBadParameter;
        }
    }
    return result;
}

OMX_ERRORTYPE crypto_deinit(Crypto *crypto) {

    OMX_ERRORTYPE result = OMX_ErrorNone;

    if (crypto->m_crypto_deinit) {
        result = (OMX_ERRORTYPE)crypto->m_crypto_deinit(&crypto->m_secure_handle);
    } else {
        GST_ERROR("Invalid method handle to OEMCryptoTerminate");
        result = OMX_ErrorBadParameter;
    }
    unload_crypto_lib(crypto);
    return result;
}

OMX_ERRORTYPE crypto_copy(Crypto *crypto, SecureCopyDir eCopyDir,
        OMX_U8 *pBuffer, unsigned long nBufferFd, OMX_U32 nBufferSize) {

    SecureCopyResult result = SECURE_COPY_SUCCESS;
    uint32 nBytesCopied = 0;

    if (crypto->m_crypto_copy == NULL) {
        GST_ERROR("Invalid method handle to OEMCryptoCopy");
        return OMX_ErrorBadParameter;
    }

    GST_DEBUG ("CryptoCopy, fd: %u, buf: %p, size: %u, byte_ct: %u, copy_dir: %d",
        (unsigned int)nBufferFd, pBuffer, (unsigned int)nBufferSize, (unsigned int)nBytesCopied, eCopyDir);
    result = crypto->m_crypto_copy(crypto->m_secure_handle, pBuffer, nBufferSize,
            nBufferFd, 0, &nBytesCopied, eCopyDir);

    if ((result != SECURE_COPY_SUCCESS) || (nBytesCopied != nBufferSize)) {
        GST_ERROR(
            "Error in CryptoCopy, fd: %u, buf: %p, size: %u, byte_ct: %u, copy_dir: %d result:%d",
            (unsigned int)nBufferFd, pBuffer, (unsigned int)nBufferSize, (unsigned int)nBytesCopied, eCopyDir, result);
        return OMX_ErrorBadParameter;
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE load_crypto_lib(Crypto *crypto) {

    OMX_ERRORTYPE result = OMX_ErrorNone;

    GST_DEBUG ("Loading crypto lib");

    crypto->m_lib_handle = dlopen(SymOEMCryptoLib, RTLD_NOW);
    if (crypto->m_lib_handle == NULL) {
        GST_ERROR("Failed to open %s, error : %s", SymOEMCryptoLib, dlerror());
        return OMX_ErrorUndefined;
    }

    crypto->m_crypto_set_appname = (Crypto_Init)dlsym(crypto->m_lib_handle, SymOEMCryptoSetAppName);
    if (crypto->m_crypto_set_appname == NULL) {
        GST_ERROR("Failed to find symbol for OEMCryptoInit: %s", dlerror());
        result = OMX_ErrorUndefined;
    }

    crypto->m_crypto_init = (Crypto_Init)dlsym(crypto->m_lib_handle, SymOEMCryptoInit);
    if (crypto->m_crypto_init == NULL) {
        GST_ERROR("Failed to find symbol for OEMCryptoInit: %s", dlerror());
        result = OMX_ErrorUndefined;
    }
    if (result == OMX_ErrorNone) {
        crypto->m_crypto_deinit = (Crypto_Deinit)dlsym(crypto->m_lib_handle, SymOEMCryptoTerminate);
        if (crypto->m_crypto_deinit == NULL) {
            GST_ERROR("Failed to find symbol for OEMCryptoTerminate: %s", dlerror());
            result = OMX_ErrorUndefined;
        }
    }
    if (result == OMX_ErrorNone) {
        crypto->m_crypto_copy = (Crypto_Copy)dlsym(crypto->m_lib_handle, SymOEMCryptoCopy);
        if (crypto->m_crypto_copy == NULL) {
            GST_ERROR("Failed to find symbol for OEMCryptoCopy: %s", dlerror());
            result = OMX_ErrorUndefined;
        }
    }
    if (result != OMX_ErrorNone) {
        unload_crypto_lib(crypto);
    }
    return result;
}

void unload_crypto_lib(Crypto *crypto) {

    if (crypto->m_lib_handle) {
        dlclose(crypto->m_lib_handle);
        crypto->m_lib_handle = NULL;
        crypto->m_secure_handle = NULL;
    }
    crypto->m_crypto_init = NULL;
    crypto->m_crypto_set_appname = NULL;
    crypto->m_crypto_deinit = NULL;
    crypto->m_crypto_copy = NULL;
}



