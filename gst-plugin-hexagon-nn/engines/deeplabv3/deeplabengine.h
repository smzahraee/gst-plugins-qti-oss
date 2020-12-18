/*
* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
*/

#ifndef DEEPLABENGINE_H
#define DEEPLABENGINE_H

#include "nnengine.h"
#include "engines/common_utils.h"

class DeepLabv3Engine : public NNEngine {
public:

  DeepLabv3Engine ()
          : NNEngine(kModelLib, kPadWidth, kPadHeight, kNumOutputs, kInFormat)
            {};

  int32_t Init(const NNSourceInfo* pSourceInfo) override;

  void DeInit() override;

private:

  int32_t PostProcess(void* outputs[], GstBuffer * gst_buffer) override;

  int32_t FillMLMeta(GstBuffer * gst_buffer) override;

  void * GenerateMeta(GstBuffer * gst_buffer);

  void GenerateRGBAImage(void* outputs[], void *buffer);

  static const std::string    kModelLib;

  // input
  static const NNImgFormat    kInFormat         = NN_FORMAT_RGB24;
  static const uint32_t       kPadWidth         = 513;
  static const uint32_t       kPadHeight        = 513;

  // output
  static const uint32_t       kNumOutputs       = 1;
  static const uint32_t       kOutBytesPerPixel = 4;
  static const uint32_t       kOutSize0         =
                                kPadWidth * kPadHeight * kOutBytesPerPixel;

  // local copy of inference result for async oprating mode
  uint8_t*                    result_buff_;
  std::mutex                  lock_;
};

#endif // DEEPLABENGINE_H
