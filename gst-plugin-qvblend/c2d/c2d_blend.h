/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __c2d_blend_H__
#define __c2d_blend_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdint.h>

#include "gbm.h"
#include "gbm_priv.h"
#include "C2DColorConverter.h"
#define ALIGN4K 4096
#define ALIGN128 128
#define ALIGN64 64
#define ALIGN32 32
#define ALIGN16 16
#define ALIGN( num, to ) (((num) + (to-1)) & (~(to-1)))

typedef struct C2DBuffer{
    int fd;
    int handle;
    int size;
    int gbm_format;
    int width;
    int height;
    int pitch;
    int meta_fd;
    struct gbm_bo *gbm_bo;
    void *ptr;
}C2DBuffer;

class C2DColorConverterBase {

public:
    virtual ~C2DColorConverterBase(){};
    virtual int convertC2D(int srcFd, void *srcBase, void * srcData, int dstFd, void *dstBase, void * dstData) = 0;
    virtual int32_t getBuffReq(int32_t port, C2DBuffReq *req) = 0;
    virtual int32_t dumpOutput(char * filename, char mode) = 0;
    virtual int SourceCrop(int x, int y, size_t srcWidth, size_t srcHeight) = 0;
    virtual int SetSourceConfigFlags(int flags) = 0;
    virtual int SetBlend(int x, int y, size_t srcWidth, size_t srcHeight, size_t dstWidth, size_t dstHeight, ColorConvertFormat srcFormat, ColorConvertFormat dstFormat) = 0;
};

typedef C2DColorConverterBase* createC2DColorConverter_t(size_t srcWidth, size_t srcHeight, size_t dstWidth, size_t dstHeight, ColorConvertFormat srcFormat, ColorConvertFormat dstFormat, int32_t flags, size_t srcStride);
typedef void destroyC2DColorConverter_t(C2DColorConverterBase*);

class c2d_blend
{
public:
    c2d_blend();
    ~c2d_blend();
    bool Init();
    bool Open(unsigned int src_height,unsigned int src_width,
              unsigned int dst_height, unsigned int dst_width,
              ColorConvertFormat src, ColorConvertFormat dest, unsigned int flag, unsigned int src_stride);
    bool Convert(int src_fd, void *src_base, void *src_data,
                 int dest_fd, void *dest_base, void *dest_data);
    bool GetBufferSize(int port,unsigned int &buf_size);
    int GetSrcFormat();
    void Close();
    bool AllocateBuffer(int port, struct C2DBuffer *buffer);
    bool FreeBuffer(struct C2DBuffer *buffer);
    int32_t DumpOutput(char * filename, char mode);
    int32_t DumpInput(char * filename, char mode);
    void SetInputCrop(int x, int y, int width, int height);
    void Blend(int x, int y,
               unsigned int src_width, unsigned int src_height,
               unsigned int dst_width, unsigned int dst_height,
               ColorConvertFormat src, ColorConvertFormat dest);

private:
    C2DColorConverter *m_c2d_conv;
    pthread_mutex_t m_lock;
    void *m_gbmhandle;
    ColorConvertFormat m_src_format;
    int m_gbm_client_fd;
    int (*gbm_bo_get_fd)(struct gbm_bo *bo);
    int (*gbm_perform )(int operation,...);
    struct gbm_bo * (*gbm_bo_create)(struct gbm_device *gbm,uint32_t width, uint32_t height,uint32_t format, uint32_t flags);
    void (*gbm_bo_destroy)(struct gbm_bo *bo);
    uint32_t (*gbm_bo_get_width)(struct gbm_bo *bo);
    uint32_t (*gbm_bo_get_height)(struct gbm_bo *bo);
    uint32_t (*gbm_bo_get_stride)(struct gbm_bo *bo);
    void (*gbm_device_destroy)(struct gbm_device *gbm);
    struct gbm_device * (*gbm_create_device)(int fd);
    struct gbm_device *m_gbm_dev;

    bool m_bInit;
};

#endif
