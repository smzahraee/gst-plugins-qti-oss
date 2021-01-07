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
#include <QC2Constants.h>

#define MAX_PENDING_WORK 16

using namespace qc2;
using namespace std::chrono_literals;

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
    mOutPendingBuffer.clear();
    mLinearPool = nullptr;
    mGraphicPool = nullptr;
}

c2_status_t C2ComponentAdapter::setListenercallback (std::unique_ptr<EventCallback> callback, c2_blocking_t mayBlock) {

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

c2_status_t C2ComponentAdapter::prepareC2Buffer(
    uint8_t* const* rawBuffer,
    uint32_t frameSize,
    std::shared_ptr<QC2Buffer> *c2Buf){

    c2_status_t result = C2_OK;
    uint32_t allocSize = 0;

    if (rawBuffer == nullptr) {
        LOG_ERROR("Inavlid buffer in prepareC2Buffer(%p)", this);
        result = C2_BAD_VALUE;
    } else {
        std::shared_ptr<QC2Buffer> buf;
        std::unique_ptr<QC2Buffer::Mapping> map;
        QC2Status err = QC2_OK;

        allocSize = ALIGN(frameSize, 4096);
        mLinearPool->setBufferSize(allocSize);
        err = mLinearPool->allocate(&buf);
        if (err != QC2_OK || buf == nullptr) {
            LOG_ERROR("Failed to allocate input buffer of size : (%d)", frameSize);
            return C2_NO_MEMORY;
        }

        map = buf->linear().map();
        memcpy_s(map->baseRW(), map->capacity(), rawBuffer, frameSize);
        buf->linear().setRange(0, frameSize);

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

c2_status_t C2ComponentAdapter::queue (
    uint8_t* const* inputBuffer,
    size_t inputBufferSize,
    C2FrameData::flags_t inputFrameFlag,
    uint64_t frame_index,
    uint64_t timestamp) {

    LOG_MESSAGE("Component(%p) work queued, Frame index : %lu, Timestamp : %lu", this, frame_index, timestamp);

    c2_status_t result = C2_OK;
    std::list<std::unique_ptr<C2Work>> workList;
    std::unique_ptr<C2Work> work = std::make_unique<C2Work>();

    work->input.flags = inputFrameFlag;
    work->input.ordinal.timestamp = timestamp;
    work->input.ordinal.frameIndex = frame_index;
    bool isEOSFrame = inputFrameFlag & C2FrameData::FLAG_END_OF_STREAM;

    work->input.buffers.clear();
    if (inputBuffer) {
        std::shared_ptr<QC2Buffer> clientBuf;

        result = prepareC2Buffer(
            inputBuffer,
            inputBufferSize,
            &clientBuf);
        if (result == C2_OK) {
            work->input.buffers.emplace_back(clientBuf->getSharedBuffer());
        } else {
            LOG_ERROR("Failed(%d) to allocate buffer", result);
        }
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

c2_status_t C2ComponentAdapter::setPoolProperty(C2BlockPool::local_id_t poolType, uint32_t width, uint32_t height, uint32_t fmt) {

    LOG_MESSAGE("Component(%p) block pool (%lu) set width: %d, height: %d fmt: %d",
                 this, poolType, width, height, fmt);

    c2_status_t ret = C2_OK;

    if (poolType == C2BlockPool::BASIC_GRAPHIC) {
        if (mGraphicPool) {
            mGraphicPool->setResolution(width, height);
            mGraphicPool->setFormat(fmt);
        } else {
            LOG_ERROR("pool not created");
            ret = C2_BAD_VALUE;
        }
    } else {
        LOG_ERROR("setPoolProperty can only be called for Graphic pool");
        ret = C2_BAD_VALUE;
    }

    return ret;
}

c2_status_t C2ComponentAdapter::flush (C2Component::flush_mode_t mode, std::list< std::unique_ptr< C2Work >> *const flushedWork) {

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
    std::shared_ptr<C2BlockPool> pool = nullptr;

    if (poolType == C2BlockPool::BASIC_LINEAR) {

        ret = android::GetCodec2BlockPool(poolType, mComp, &pool);
        if (ret != C2_OK || pool == nullptr) {
            return ret;
        }

        mLinearPool = std::make_unique<QC2LinearBufferPool>(pool, (MemoryUsage::HW_CODEC_WRITE | MemoryUsage::HW_CODEC_READ));

    } else if (poolType == C2BlockPool::BASIC_GRAPHIC) {

        ret = android::GetCodec2BlockPool(poolType, mComp, &pool);
        if (ret != C2_OK || pool == nullptr) {
            return ret;
        }

        mGraphicPool = std::make_shared<QC2GraphicBufferPoolImpl>(pool, (MemoryUsage::HW_CODEC_WRITE | MemoryUsage::HW_CODEC_READ));
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
        std::shared_ptr<QC2Buffer> outBuffer = nullptr;

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

            LOG_MESSAGE("Component(%p) output buffer available, Frame index : %lu, Timestamp : %lu",
                this, bufferIdx, worklet->output.ordinal.timestamp.peeku());

            outBuffer = QC2Buffer::CreateFromC2Buffer(buffer);

            if (mMapBufferToCpu == false) {
                mOutPendingBuffer[bufferIdx] = outBuffer;
            }

            mCallback->onOutputBufferAvailable(outBuffer, bufferIdx, timestamp, outputFrameFlag);
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
    std::map<uint64_t, std::shared_ptr<QC2Buffer>>::iterator it;

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// C2Component::Listener
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
