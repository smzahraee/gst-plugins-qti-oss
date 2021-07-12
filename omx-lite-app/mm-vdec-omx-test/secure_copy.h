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

#ifndef _SECURE_COPY_H
#define _SECURE_COPY_H

#include <stdint.h>
#include <stddef.h>
#include <dlfcn.h>

enum secure_copy_direction {
    SCD_COPY_NONSECURE_TO_SECURE = 0,
    SCD_COPY_SECURE_TO_NONSECURE,
    SCD_COPY_INVALID_DIR
};

class secure_copy {
public:
    friend bool do_secure_copy(uint8_t *buf, size_t *size, int fd, int direction);

private:
    static secure_copy* instance(void) {
        static secure_copy instance;
        return &instance;
    }

    /* On success, returen true, or else return false. */
    bool copy(uint8_t *non_sec_buf, size_t non_sec_buf_len, int sec_buf_fd,
              size_t sec_buf_offset, size_t *sec_buf_len, int copy_dir);

    secure_copy();
    secure_copy(const secure_copy&) = delete;
    secure_copy& operator=(const secure_copy&) = delete;
    ~secure_copy();

    int load_lib(void);
    void unload_lib(void) {
        secure_copy_deinit = nullptr;
        secure_copy_func = nullptr;
        secure_copy_init = nullptr;
        secure_copy_set_app_name = nullptr;
        secure_handle = nullptr;
        if (lib_cc_handle) {
            dlclose(lib_cc_handle);
            lib_cc_handle = nullptr;
        }
    }

    void *lib_cc_handle; /* libcontentcopy handle */
    void *secure_handle; /* QSEECom_handle */

    int (*secure_copy_set_app_name)(const char *name);
    int (*secure_copy_init)(void **handle);
    int (*secure_copy_func)(void *handle,
        uint8_t *non_sec_buf, uint32_t non_sec_buf_len,
        uint32_t sec_buf_fd, uint32_t sec_buf_offset, uint32_t *sec_buf_len,
        int copy_dir);
    int (*secure_copy_deinit)(void **handle);
};

bool do_secure_copy(uint8_t *buf, size_t *size, int fd, int direction);

static inline bool
copy_to_secure_buffer(const uint8_t *buf, size_t *size, int fd)
{
  return do_secure_copy((uint8_t *)buf, size, fd, SCD_COPY_NONSECURE_TO_SECURE);
}

static inline bool
copy_from_secure_buffer(uint8_t *buf, size_t *size, int fd)
{
  return do_secure_copy(buf, size, fd, SCD_COPY_SECURE_TO_NONSECURE);
}

#endif //#ifndef _SECURE_COPY_H
