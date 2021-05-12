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

#include "c2ComponentAdapter.h"

#include <chrono>
#include <C2PlatformSupport.h>
#include <media/msm_media_info.h>
#include <gst/gst.h>
#include <C2AllocatorGBM.h>

GST_DEBUG_CATEGORY_EXTERN (gst_qticodec2wrapper_debug);
#define GST_CAT_DEFAULT gst_qticodec2wrapper_debug

/* Currently, size of input queue is 6 in video driver.
 * If count of pending works are more than 6, it causes queue overflow issue.
 */
#define MAX_PENDING_WORK 6

using namespace std::chrono_literals;

std::shared_ptr<C2Buffer> createLinearBuffer(const std::shared_ptr<C2LinearBlock> &block) {
  return C2Buffer::CreateLinearBuffer(block->share(block->offset(), block->size(), ::C2Fence()));
}

std::shared_ptr<C2Buffer> createGraphicBuffer( const std::shared_ptr<C2GraphicBlock> &block) {
  return C2Buffer::CreateGraphicBuffer(block->share(C2Rect(block->width(), block->height()), ::C2Fence()));
}

namespace QTI {

C2ComponentAdapter::C2ComponentAdapter(std::shared_ptr<C2Component> comp) {

    LOG_MESSAGE("Component(%p) created", this);

    mComp = std::move(comp);
    mIntf = nullptr;
    mListener = nullptr;
    mCallback = nullptr;
    mLinearPool = nullptr;
    mGraphicPool = nullptr;
    mMapBufferToCpu = false;
    mNumPendingWorks = 0;
}

C2ComponentAdapter::~C2ComponentAdapter() {

    LOG_MESSAGE("Component(%p) destroyed", this);

    mComp = nullptr;
    mIntf = nullptr;
    mListener = nullptr;
    mCallback = nullptr;
    mInPendingBuffer.clear();
    mOutPendingBuffer.clear();
    mLinearPool = nullptr;
    mGraphicPool = nullptr;
}

c2_status_t C2ComponentAdapter::setListenercallback (std::unique_ptr<EventCallback> callback,
    c2_blocking_t mayBlock) {

    LOG_MESSAGE("Component(%p) listener set", this);

    c2_status_t result = C2_NO_INIT;

    if (callback != NULL){
        mListener = std::shared_ptr<C2Component::Listener>(new C2ComponentListenerAdapter(this));
        result = mComp->setListener_vb(mListener, mayBlock);
    }

    if (result == C2_OK) {
        mCallback = std::move(callback);
    }

    return result;
}

c2_status_t C2ComponentAdapter::writePlane(uint8_t *dest, BufferDescriptor *buffer_info) {
    c2_status_t result = C2_OK;
    uint8_t *dst = dest;
    uint8_t *src = buffer_info->data;

    if (dst == nullptr || src == nullptr) {
        LOG_ERROR("Inavlid buffer in writePlane(%p)", this);
        return C2_BAD_VALUE;
    }

    /*TODO: add support for other color formats */
    if (buffer_info->format == GST_VIDEO_FORMAT_NV12) {
        uint32_t width = buffer_info->width;
        uint32_t height = buffer_info->height;
        uint32_t y_stride = VENUS_Y_STRIDE(COLOR_FMT_NV12, width);
        uint32_t uv_stride = VENUS_UV_STRIDE(COLOR_FMT_NV12, width);
        uint32_t y_scanlines = VENUS_Y_SCANLINES(COLOR_FMT_NV12, height);

        for (int i = 0; i < height; i ++) {
            memcpy(dst, src, width);
            dst += y_stride;
            src += width;
        }

        uint32_t offset = y_stride * y_scanlines;
        dst = dest + offset;

        for (int i = 0; i < height/2; i ++) {
            memcpy(dst, src, width);
            dst += uv_stride;
            src += width;
        }
    } else {
        result = C2_BAD_VALUE;
    }

    return result;
}

c2_status_t C2ComponentAdapter::prepareC2Buffer (std::shared_ptr<C2Buffer> *c2Buf, BufferDescriptor* buffer) {
    uint8_t* rawBuffer = buffer->data;
    uint8_t* destBuffer = nullptr;
    uint32_t frameSize = buffer->size;
    C2BlockPool::local_id_t poolType = buffer->pool_type;
    c2_status_t result = C2_OK;
    uint32_t allocSize = 0;

    if (rawBuffer == nullptr) {
        LOG_ERROR("Inavlid buffer in prepareC2Buffer(%p)", this);
        result = C2_BAD_VALUE;
    } else {
        std::shared_ptr<C2LinearBlock> linear_block;
        std::shared_ptr<C2GraphicBlock> graphic_block;

        std::shared_ptr<C2Buffer> buf;
        c2_status_t err = C2_OK;
        C2MemoryUsage usage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };

        if (poolType == C2BlockPool::BASIC_LINEAR) {
            allocSize = ALIGN(frameSize, 4096);
            err = mLinearPool->fetchLinearBlock (allocSize, usage, &linear_block);
            if (err != C2_OK || linear_block == nullptr) {
                LOG_ERROR("Linear pool failed to allocate input buffer of size : (%d)", frameSize);
                return C2_NO_MEMORY;
            }

            C2WriteView view = linear_block->map().get();
            if (view.error() != C2_OK) {
                LOG_ERROR("C2LinearBlock::map() failed : %d", view.error());
                return C2_NO_MEMORY;
            }
            destBuffer = view.base();
            memcpy(destBuffer, rawBuffer, frameSize);
            linear_block->mSize = frameSize;
            buf = createLinearBuffer(linear_block);
        } else if (poolType == C2BlockPool::BASIC_GRAPHIC) {
          if (mGraphicPool) {
              // TODO support NV12_UBWC input by usage
              usage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };
              err = mGraphicPool->fetchGraphicBlock (buffer->width, buffer->height,
                      gst_to_c2_gbmformat(buffer->format), usage, &graphic_block);
              C2GraphicView view(graphic_block->map().get());
              if (view.error() != C2_OK) {
                  LOG_ERROR("C2GraphicBlock::map failed: %d", view.error());
                  return C2_NO_MEMORY;
              }

              destBuffer = (guint8*) view.data()[0];

              if (C2_OK != writePlane(destBuffer, buffer)) {
                  LOG_ERROR("failed to write planes for graphic buffer");
                  return C2_NO_MEMORY;
              }

              buf = createGraphicBuffer(graphic_block);
              if (err != C2_OK || buf == nullptr) {
                  LOG_ERROR("Graphic pool failed to allocate input buffer");
                  return C2_NO_MEMORY;
              }
          }
        }

        *c2Buf = buf;
    }

    return result;
}

c2_status_t C2ComponentAdapter::waitForProgressOrStateChange(
    uint32_t maxPendingWorks,
    uint32_t timeoutMs) {

    std::unique_lock<std::mutex> ul(mLock);
    LOG_MESSAGE("waitForProgressOrStateChange: pending = %u", mNumPendingWorks);

    while (mNumPendingWorks > maxPendingWorks) {
        if (timeoutMs > 0) {
            if (mCondition.wait_for(ul, timeoutMs*1ms) == std::cv_status::timeout) {
                LOG_ERROR("Timed-out waiting for work / state-transition (pending=%u)",
                        mNumPendingWorks);
                return C2_TIMED_OUT;
            } else {
                LOG_MESSAGE("wait done");
                break;
            }
        }
        else if (timeoutMs == 0) {
            mCondition.wait(ul);
        }
    }

    return C2_OK;
}

std::shared_ptr<C2Buffer> C2ComponentAdapter::alloc(BufferDescriptor* buffer) {
    c2_status_t err = C2_OK;
    std::shared_ptr<C2Buffer> buf;

    /* TODO: add support for linear buffer */
    if (buffer->pool_type == BUFFER_POOL_BASIC_GRAPHIC) {
        std::shared_ptr<C2GraphicBlock> graphic_block;
        C2MemoryUsage usage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };
        if (mGraphicPool) {
              // TODO support NV12_UBWC input by usage
              usage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE};
              err = mGraphicPool->fetchGraphicBlock (buffer->width, buffer->height,
                      gst_to_c2_gbmformat(buffer->format), usage, &graphic_block);
              C2GraphicView view(graphic_block->map().get());
              if (view.error() != C2_OK) {
                  LOG_ERROR("C2GraphicBlock::map failed: %d", view.error());
                  return NULL;
              }
              buf = createGraphicBuffer(graphic_block);
              if (err != C2_OK || buf == nullptr) {
                  LOG_ERROR("Graphic pool failed to allocate input buffer");
                  return NULL;
              } else {
                /* ref the buffer and store it. When the fd is queued,
                 * we can find the C2buffer with the input fd
                 * */
                uint32_t fd = graphic_block->handle()->data[0];
                mInPendingBuffer[fd] = buf;
                buffer->fd = fd;
                guint32 width = 0;
                guint32 height = 0;
                guint32 format = 0;
                guint32 stride = 0;
                guint64 usage = 0;
                guint32 size = 0;

                _UnwrapNativeCodec2GBMMetadata (graphic_block->handle(), &width, &height, &format, &usage, &stride, &size);
                buffer->capacity = size;
                LOG_MESSAGE("allocated C2Buffer, fd: %d capacity: %d", fd, buffer->capacity);
            }
        } else {
            LOG_ERROR("Graphic pool is not created");
            return NULL;
        }
    } else {
        LOG_ERROR("Unsupported pool type: %u", buffer->pool_type);
        return NULL;
    }

    return buf;
}

c2_status_t C2ComponentAdapter::queue (BufferDescriptor* buffer)
{
    uint8_t* inputBuffer = buffer->data;
    gint32 fd = buffer->fd;
    size_t inputBufferSize = buffer->size;
    C2FrameData::flags_t inputFrameFlag = QTI::toC2Flag(buffer->flag);
    uint64_t frame_index = buffer->index;
    uint64_t timestamp = buffer->timestamp;
    C2BlockPool::local_id_t poolType = toC2BufferPoolType(buffer->pool_type);
    gint width = buffer->width;
    gint height = buffer->height;

    LOG_MESSAGE("Component(%p) work queued, Frame index : %lu, Timestamp : %lu",
        this, frame_index, timestamp);

    c2_status_t result = C2_OK;
    std::list<std::unique_ptr<C2Work>> workList;
    std::unique_ptr<C2Work> work = std::make_unique<C2Work>();

    work->input.flags = inputFrameFlag;
    work->input.ordinal.timestamp = timestamp;
    work->input.ordinal.frameIndex = frame_index;
    bool isEOSFrame = inputFrameFlag & C2FrameData::FLAG_END_OF_STREAM;

    work->input.buffers.clear();

    /* check if input buffer contains fd/va and decide if we need to
     * allocate a new C2 buffer or not */
    if (fd > 0) {
        std::map<uint64_t, std::shared_ptr<C2Buffer>>::iterator it;

        /* Find the buffer with fd */
        it = mInPendingBuffer.find(fd);
        if (it != mInPendingBuffer.end()) {
            std::shared_ptr<C2Buffer> buf = it->second;
            work->input.buffers.emplace_back(buf);
            result = C2_OK;
        } else {
            LOG_ERROR("Buffer fd(%u) not found", fd);
            return C2_NOT_FOUND;
        }
    } else if (inputBuffer) {
        std::shared_ptr<C2Buffer> clientBuf;

        result = prepareC2Buffer(&clientBuf, buffer);
        if (result == C2_OK) {
            work->input.buffers.emplace_back(clientBuf);
        } else {
            LOG_ERROR("Failed(%d) to allocate buffer", result);
            return C2_BAD_VALUE;
        }
    } else if (isEOSFrame) {
        LOG_MESSAGE ("queue EOS frame");
    } else {
      LOG_ERROR ("invalid buffer decriptor");
      return C2_BAD_VALUE;
    }

    work->worklets.clear();
    work->worklets.emplace_back(new C2Worklet);
    workList.push_back(std::move(work));

    if (!isEOSFrame) {
        waitForProgressOrStateChange(MAX_PENDING_WORK, 0);
    }
    else {
        LOG_MESSAGE("EOS reached");
    }

    result = mComp->queue_nb(&workList);
    if (result != C2_OK) {
        LOG_ERROR("Failed to queue work");
    }

    std::unique_lock<std::mutex> ul(mLock);
    mNumPendingWorks ++;

    return result;
}

c2_status_t C2ComponentAdapter::flush (C2Component::flush_mode_t mode,
    std::list< std::unique_ptr< C2Work >> *const flushedWork) {

    LOG_MESSAGE("Component(%p) flushed", this);

    c2_status_t result = C2_OK;
    UNUSED(mode);
    UNUSED(flushedWork);

    return result;
}

c2_status_t C2ComponentAdapter::drain (C2Component::drain_mode_t mode) {

    LOG_MESSAGE("Component(%p) drain", this);

    c2_status_t result = C2_OK;
    UNUSED(mode);

    return result;
}

c2_status_t C2ComponentAdapter::start () {

    LOG_MESSAGE("Component(%p) start", this);

    return mComp->start();
}

c2_status_t C2ComponentAdapter::stop () {

    LOG_MESSAGE("Component(%p) stop", this);

    return mComp->stop();
}

c2_status_t C2ComponentAdapter::reset () {

    LOG_MESSAGE("Component(%p) reset", this);

    return mComp->reset();
}

c2_status_t C2ComponentAdapter::release () {

    LOG_MESSAGE("Component(%p) release", this);

    return mComp->release();
}

C2ComponentInterfaceAdapter* C2ComponentAdapter::intf () {

    LOG_MESSAGE("Component(%p) interface created", this);

    if (mComp) {
        std::shared_ptr<C2ComponentInterface> compIntf = nullptr;

        compIntf = mComp->intf();
        mIntf = std::shared_ptr<C2ComponentInterfaceAdapter>(new C2ComponentInterfaceAdapter(compIntf));
    }

    return (mIntf == NULL) ? NULL : mIntf.get();
}

c2_status_t C2ComponentAdapter::createBlockpool(C2BlockPool::local_id_t poolType) {

    LOG_MESSAGE("Component(%p) block pool (%lu) allocated", this, poolType);

    c2_status_t ret = C2_OK;

    if (poolType == C2BlockPool::BASIC_LINEAR) {
        ret = android::GetCodec2BlockPool(poolType, mComp, &mLinearPool);
        if (ret != C2_OK || mLinearPool == nullptr) {
            return ret;
        }
    } else if (poolType == C2BlockPool::BASIC_GRAPHIC) {
        ret = android::GetCodec2BlockPool(poolType, mComp, &mGraphicPool);
        if (ret != C2_OK || mGraphicPool == nullptr) {
            return ret;
        }
    }

    if (ret != C2_OK) {
        LOG_ERROR("Failed (%d) to create block pool (%lu)",ret, poolType);
    }

    return ret;
}

void C2ComponentAdapter::handleWorkDone(
    std::weak_ptr<C2Component> component,
    std::list<std::unique_ptr<C2Work>> workItems) {

    LOG_MESSAGE("Component(%p) work done", this);

    while (!workItems.empty()) {
        std::unique_ptr<C2Work> work = std::move(workItems.front());

        workItems.pop_front();
        if (!work) {
            continue;
        }

        if (work->worklets.empty()) {
            LOG_MESSAGE("Component(%p) worklet empty", this);
            continue;
        }

        if (work->result == C2_NOT_FOUND) {
            LOG_MESSAGE("No output for component(%p)", this);
            break;
        }

        // Work failed to complete
        if (work->result != C2_OK) {
            LOG_MESSAGE("Failed to generate output for component(%p)", this);
            break;
        }

        const std::unique_ptr<C2Worklet> &worklet = work->worklets.front();
        std::shared_ptr<C2Buffer> buffer = nullptr;
        uint64_t bufferIdx = 0;
        C2FrameData::flags_t outputFrameFlag = worklet->output.flags;
        uint64_t timestamp = worklet->output.ordinal.timestamp.peeku();

        // Expected only one output stream.
        if (worklet->output.buffers.size() == 1u) {

            buffer = worklet->output.buffers[0];
            bufferIdx = worklet->output.ordinal.frameIndex.peeku();
            if (!buffer) {
                LOG_ERROR("Invalid buffer");
            }

            LOG_MESSAGE("Component(%p) output buffer available, Frame index : %lu, Timestamp : %lu, flag: %x",
                this, bufferIdx, worklet->output.ordinal.timestamp.peeku(), outputFrameFlag);

            // ref count ++
            mOutPendingBuffer[bufferIdx] = buffer;

            mCallback->onOutputBufferAvailable(buffer, bufferIdx, timestamp, outputFrameFlag);
            std::unique_lock<std::mutex> ul(mLock);
            mNumPendingWorks --;
            mCondition.notify_one();
        }
        else {

            if (outputFrameFlag & C2FrameData::FLAG_END_OF_STREAM) {
                LOG_MESSAGE("Component(%p) reached EOS on output", this);
                mCallback->onOutputBufferAvailable(NULL, bufferIdx, timestamp, outputFrameFlag);
            }
            else {
                LOG_ERROR("Incorrect number of output buffers: %lu", worklet->output.buffers.size());
            }
            std::unique_lock<std::mutex> ul(mLock);
            mNumPendingWorks --;
            mCondition.notify_one();
            break;
        }
    }
}

void C2ComponentAdapter::handleTripped(
    std::weak_ptr<C2Component> component,
    std::vector<std::shared_ptr<C2SettingResult>> settingResult) {

    LOG_MESSAGE("Component(%p) work tripped", this);

    UNUSED(component);

    for (auto& f : settingResult) {
        mCallback->onTripped(static_cast<uint32_t>(f->failure));
    }
}

void C2ComponentAdapter::handleError(std::weak_ptr<C2Component> component, uint32_t errorCode) {

    LOG_MESSAGE("Component(%p) work failed", this);

    UNUSED(component);

    mCallback->onError(errorCode);
}

c2_status_t C2ComponentAdapter::setCompStore (std::weak_ptr<C2ComponentStore> store) {

    LOG_MESSAGE("Component store for component(%p) set", this);

    c2_status_t result = C2_BAD_VALUE;
    if (!store.expired()){
        mStore = store;
        result =  C2_OK;
    }
    return result;
}

c2_status_t C2ComponentAdapter::freeOutputBuffer (uint64_t bufferIdx) {

    LOG_MESSAGE("Freeing component(%p) output buffer(%lu)", this, bufferIdx);

    c2_status_t result = C2_BAD_VALUE;
    std::map<uint64_t, std::shared_ptr<C2Buffer>>::iterator it;

    it = mOutPendingBuffer.find(bufferIdx);
    if (it != mOutPendingBuffer.end()) {
        mOutPendingBuffer.erase (it);
        result = C2_OK;

    } else {
        LOG_MESSAGE("Buffer index(%lu) not found", bufferIdx);
    }

    return result;
}

c2_status_t C2ComponentAdapter::setMapBufferToCpu (bool enable) {

    c2_status_t c2Status = C2_NO_INIT;

    mMapBufferToCpu = enable;

    if (mCallback) {
        mCallback->setMapBufferToCpu(mMapBufferToCpu);

        c2Status = C2_OK;
    }

    return c2Status;
}

C2ComponentListenerAdapter::C2ComponentListenerAdapter(C2ComponentAdapter* comp) {

    mComp = comp;
}

C2ComponentListenerAdapter::~C2ComponentListenerAdapter() {

    mComp = nullptr;
}

void C2ComponentListenerAdapter::onWorkDone_nb(
    std::weak_ptr<C2Component> component,
    std::list<std::unique_ptr<C2Work>> workItems) {

    LOG_MESSAGE("Component listener (%p) onWorkDone_nb", this);

    if (mComp) {
        mComp->handleWorkDone(component, std::move(workItems));
    }
}

void C2ComponentListenerAdapter::onTripped_nb(
    std::weak_ptr<C2Component> component,
    std::vector<std::shared_ptr<C2SettingResult>> settingResult) {

    LOG_MESSAGE("Component listener (%p) onTripped_nb", this);

    if (mComp) {
        mComp->handleTripped(component, settingResult);
    }
}

void C2ComponentListenerAdapter::onError_nb(std::weak_ptr<C2Component> component, uint32_t errorCode) {

    LOG_MESSAGE("Component listener (%p) onError_nb", this);

    if (mComp) {
        mComp->handleError(component, errorCode);
    }
}

} // namespace QTI
