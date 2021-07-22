/*
* Copyright (c) 2021, The Linux Foundation. All rights reserved.
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

#ifndef __UTILS_H__
#define __UTILS_H__

#include "wrapper_utils.h"
#include <types.h>
#include <C2Config.h>
#include <codec2wrapper.h>

namespace QTI {

uint32_t toC2InterlaceType(INTERLACE_MODE_TYPE interlace_type) {
    uint32_t type = 0;

    switch(interlace_type){
        case INTERLACE_MODE_PROGRESSIVE : {
            type = C2_INTERLACE_MODE_PROGRESSIVE;
            break;
        }
        case INTERLACE_MODE_INTERLEAVED_TOP_FIRST : {
            type = C2_INTERLACE_MODE_INTERLEAVED_TOP_FIRST;
            break;
        }
        case INTERLACE_MODE_INTERLEAVED_BOTTOM_FIRST : {
            type = C2_INTERLACE_MODE_INTERLEAVED_BOTTOM_FIRST;
            break;
        }
        case INTERLACE_MODE_FIELD_TOP_FIRST : {
            type = C2_INTERLACE_MODE_FIELD_TOP_FIRST;
            break;
        }
        case INTERLACE_MODE_FIELD_BOTTOM_FIRST : {
            type = C2_INTERLACE_MODE_FIELD_BOTTOM_FIRST;
            break;
        }
        default:{
            LOG_ERROR("Invalid Type");
            break;
        }
    }

    return type;
}

C2BlockPool::local_id_t toC2BufferPoolType(BUFFER_POOL_TYPE pool_type) {

    C2BlockPool::local_id_t type =  C2BlockPool::BASIC_LINEAR;

    switch(pool_type){
        case BUFFER_POOL_BASIC_LINEAR:{
            type = C2BlockPool::BASIC_LINEAR;
            break;
        }
        case BUFFER_POOL_BASIC_GRAPHIC:{
            type = C2BlockPool::BASIC_GRAPHIC;
            break;
        }
        default:{
            LOG_ERROR("Invalid Type");
            break;
        }
    }

    return type;
}

c2_blocking_t toC2BlocingType(BLOCK_MODE_TYPE block_type) {

    c2_blocking_t type =  C2_DONT_BLOCK;

    switch(block_type){
        case BLOCK_MODE_DONT_BLOCK:{
            type = C2_DONT_BLOCK;
            break;
        }
        case BLOCK_MODE_MAY_BLOCK:{
            type = C2_MAY_BLOCK;
            break;
        }
        default:{
            LOG_ERROR("Invalid Type");
            break;
        }
    }

    return type;
}

C2Component::drain_mode_t toC2DrainMode(DRAIN_MODE_TYPE mode) {

    C2Component::drain_mode_t drainMode = C2Component::DRAIN_COMPONENT_NO_EOS;

    switch (mode){
        case DRAIN_MODE_COMPONENT_WITH_EOS : {
            drainMode = C2Component::DRAIN_COMPONENT_WITH_EOS;
            break;
        }
        case DRAIN_MODE_COMPONENT_NO_EOS : {
            drainMode = C2Component::DRAIN_COMPONENT_NO_EOS;
            break;
        }
        case DRAIN_MODE_CHAIN : {
            drainMode = C2Component::DRAIN_CHAIN;
            break;
        }
        default:{
            LOG_ERROR("Invalid Type");
            break;
        }
    }

    return drainMode;
}

C2Component::flush_mode_t toC2FlushMode (FLUSH_MODE_TYPE mode){

    C2Component::flush_mode_t flushMode = C2Component::FLUSH_COMPONENT;

    switch (mode){
        case FLUSH_MODE_COMPONENT : {
            flushMode = C2Component::FLUSH_COMPONENT;
            break;
        }
        case FLUSH_MODE_CHAIN : {
            flushMode = C2Component::FLUSH_CHAIN;
            break;
        }
        default:{
            LOG_ERROR("Invalid Mode");
        }
    }

    return flushMode;
}

uint32_t toC2RateControlMode (RC_MODE_TYPE mode){
    uint32_t rcMode = 0x7F000000; //RC_MODE_EXT_DISABLE

    switch (mode) {
        case RC_OFF: {
            rcMode = 0x7F000000; //RC_MODE_EXT_DISABLE
            break;
        }
        case RC_CONST : {
            rcMode = C2Config::BITRATE_CONST;
            break;
        }
        case RC_CBR_VFR : {
            rcMode = C2Config::BITRATE_CONST_SKIP_ALLOWED;
            break;
        }
        case RC_VBR_CFR : {
            rcMode = C2Config::BITRATE_VARIABLE;
            break;
        }
        case RC_VBR_VFR : {
            rcMode = C2Config::BITRATE_VARIABLE_SKIP_ALLOWED;
            break;
        }
        default : {
            LOG_ERROR("Invalid RC Mode: %d", mode);
        }
    }

    return rcMode;
}

FLAG_TYPE toWrapperFlag(C2FrameData::flags_t flag) {
    uint32_t result = 0;

    if (C2FrameData::FLAG_DROP_FRAME & flag) {
        result |= FLAG_TYPE_DROP_FRAME;
    }
    if (C2FrameData::FLAG_END_OF_STREAM & flag) {
        result |= FLAG_TYPE_END_OF_STREAM;
    }
    if (C2FrameData::FLAG_INCOMPLETE & flag) {
        result |= FLAG_TYPE_INCOMPLETE;
    }
    if (C2FrameData::FLAG_CODEC_CONFIG & flag) {
        result |= FLAG_TYPE_CODEC_CONFIG;
    }

    return static_cast<FLAG_TYPE>(result);
}

C2FrameData::flags_t toC2Flag(FLAG_TYPE flag) {
    uint32_t result = 0;

    if (FLAG_TYPE_DROP_FRAME & flag) {
        result |= C2FrameData::FLAG_DROP_FRAME;
    }
    if (FLAG_TYPE_END_OF_STREAM & flag) {
        result |= C2FrameData::FLAG_END_OF_STREAM;
    }
    if (FLAG_TYPE_INCOMPLETE & flag) {
        result |= C2FrameData::FLAG_INCOMPLETE;
    }
    if (FLAG_TYPE_CODEC_CONFIG & flag) {
        result |= C2FrameData::FLAG_CODEC_CONFIG;
    }

    return static_cast<C2FrameData::flags_t>(result);
}

uint32_t toC2PixelFormat(PIXEL_FORMAT_TYPE pixel) {
    uint32_t result = 0;

    switch(pixel) {
        case PIXEL_FORMAT_NV12_LINEAR:{
            result = C2_PIXEL_FORMAT_VENUS_NV12;
            break;
        }
        case PIXEL_FORMAT_NV12_UBWC:{
            result = C2_PIXEL_FORMAT_VENUS_NV12_UBWC;
            break;
        }
        case PIXEL_FORMAT_RGBA_8888:{
            result = C2_PIXEL_FORMAT_RGBA8888;
            break;
        }
        case PIXEL_FORMAT_YV12 : {
            result = C2_PIXEL_FORMAT_YV12;
            break;
        }
        default: {
            LOG_ERROR("unsupported pixel format!");
            break;
        }
    }

    return result;
}

guint32
gst_to_c2_gbmformat (GstVideoFormat format) {
  guint32 result = 0;

  switch(format) {
    case GST_VIDEO_FORMAT_NV12 :
    case GST_VIDEO_FORMAT_NV12_UBWC :
      result = GBM_FORMAT_NV12;
      break;
    default:
      LOG_WARNING("unsupported video format:%s", gst_video_format_to_string(format));
      break;
  }

  return result;
}

guint32 to_c2_gbm_ubwc_flag (GstVideoFormat format) {
  guint32 result = 0;

  switch(format) {
    case GST_VIDEO_FORMAT_NV12_UBWC :
      result = GBM_BO_USAGE_UBWC_ALIGNED_QTI;
      break;
    default:
      break;
  }

  return result;
}

} // namespace QTI

#endif /* __UTILS_H__ */