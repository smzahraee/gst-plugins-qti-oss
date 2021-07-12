/*-------------------------------------------------------------------
Copyright (c) 2021, The Linux Foundation. All rights reserved.

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

#include "secure_copy.h"

#include <iostream>
using namespace std;

/* Secure content copy library interfaces. */
#define SCC_LibName "libcontentcopy.so"
#define SCC_AppName "smpcpyap64"
#define SCC_SetAppName "Content_Protection_Set_AppName"
#define SCC_Init "Content_Protection_Copy_Init"
#define SCC_Terminate "Content_Protection_Copy_Terminate"
#define SCC_Copy "Content_Protection_Copy"

typedef int (*secure_copy_set_app_name_t)(const char *name);
typedef int (*secure_copy_init_t)(void **handle);
typedef int (*secure_copy_func_t)(void *handle,
            uint8_t *non_sec_buf, uint32_t non_sec_buf_len,
            uint32_t sec_buf_fd, uint32_t sec_buf_offset, uint32_t *sec_buf_len,
            int copy_dir);
typedef int (*secure_copy_deinit_t)(void **handle);

secure_copy::secure_copy() :
        lib_cc_handle(nullptr), secure_handle(nullptr), secure_copy_set_app_name(nullptr),
        secure_copy_init(nullptr), secure_copy_func(nullptr), secure_copy_deinit(nullptr)
{
    int ret = load_lib();
    if (ret)
        goto unload;

    ret = (*secure_copy_set_app_name)(SCC_AppName);
    if (ret) {
        cerr << "Falied to set app name " << SCC_AppName << "ret=" << ret << endl;
        goto unload;
    }

    ret = (*secure_copy_init)(&secure_handle);
    if (ret) {
        cerr << "Falied to init " << SCC_LibName << "ret=" << ret << endl;
        goto unload;
    }

    return;

unload:
    unload_lib();
}

bool
secure_copy::copy(uint8_t *non_sec_buf, size_t non_sec_buf_len, int sec_buf_fd,
                  size_t sec_buf_offset, size_t *sec_buf_len, int copy_dir)
{
    if (nullptr == secure_copy_func)
        return false;

    uint32_t len = (uint32_t)non_sec_buf_len;
    uint32_t off = (uint32_t)sec_buf_offset;
    uint32_t sec_fd = (uint32_t)sec_buf_fd;
    uint32_t *sec_len = (uint32_t *)sec_buf_len;

    int ret = (*secure_copy_func)(secure_handle, non_sec_buf, len,
                                  sec_fd, off, sec_len, copy_dir);
    if (ret) {
        cerr << "Secure copy failed, ret=" << ret << endl;
        return false;
    }

    return true;
}

bool do_secure_copy(uint8_t *buf, size_t *size, int fd, int direction)
{
  secure_copy *sc = secure_copy::instance();
  size_t size_in = *size;

  bool ret = sc->copy(buf, *size, fd, 0, size, direction);
  printf("%s: size=%lu, copied=%lu, fd=%d, direction=%d, ret=%d\n",
         __func__, size_in, *size, fd, direction, ret);

  return ret;
}

secure_copy::~secure_copy()
{
    if (secure_copy_deinit && secure_handle)
        (*secure_copy_deinit)(&secure_handle);
    unload_lib();
}

int secure_copy::load_lib(void)
{
    lib_cc_handle = dlopen(SCC_LibName, RTLD_NOW);
    if (nullptr == lib_cc_handle) {
        cerr << "Falied to open " << SCC_LibName << ", " << dlerror() << endl;
        return -1;
    }

    secure_copy_set_app_name = (secure_copy_set_app_name_t)dlsym(lib_cc_handle, SCC_SetAppName);
    if (nullptr == secure_copy_set_app_name) {
        cerr<< "Failed to get symbol " << SCC_SetAppName << ", " << dlerror() << endl;
        return -1;
    }

    secure_copy_init = (secure_copy_init_t)dlsym(lib_cc_handle, SCC_Init);
    if (nullptr == secure_copy_init) {
        cerr<< "Failed to get symbol " << SCC_Init << ", " << dlerror() << endl;
        return -1;
    }

    secure_copy_deinit = (secure_copy_deinit_t)dlsym(lib_cc_handle, SCC_Terminate);
    if (nullptr == secure_copy_deinit) {
        cerr<< "Failed to get symbol " << SCC_Terminate << ", " << dlerror() << endl;
        return -1;
    }

    secure_copy_func = (secure_copy_func_t)dlsym(lib_cc_handle, SCC_Copy);
    if (nullptr == secure_copy_func) {
        cerr<< "Failed to get symbol " << SCC_Copy << ", " << dlerror() << endl;
        return -1;
    }

    return 0;
}
