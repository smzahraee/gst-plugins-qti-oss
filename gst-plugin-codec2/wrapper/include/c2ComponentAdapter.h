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

#ifndef __C2COMPONENTADAPTER_H__
#define __C2COMPONENTADAPTER_H__

#include "types.h"
#include "c2ComponentInterfaceAdapter.h"

#include <C2Component.h>
#include <mutex>
#include <map>
#include <condition_variable>
#include <QC2Buffer.h>

namespace QTI {

class C2ComponentAdapter;

class C2ComponentListenerAdapter : public C2Component::Listener {

public:
    C2ComponentListenerAdapter(C2ComponentAdapter* comp);
    ~C2ComponentListenerAdapter();

    /* Methods implementing C2Component::Listener */
    void onWorkDone_nb (std::weak_ptr<C2Component> component, std::list<std::unique_ptr<C2Work>> workItems) override;
    void onTripped_nb (std::weak_ptr<C2Component> component, std::vector<std::shared_ptr<C2SettingResult>> settingResult) override;
    void onError_nb (std::weak_ptr<C2Component> component, uint32_t errorCode) override;

private:
    C2ComponentAdapter* mComp;
};

class C2ComponentAdapter {

public:
    C2ComponentAdapter(std::shared_ptr<C2Component> comp);
    ~C2ComponentAdapter();

    /* Methods implementing C2Component */
    std::shared_ptr<QC2Buffer> alloc(C2BlockPool::local_id_t type);

    /* Queue buffer with va (copy) */
    c2_status_t queue (
        uint8_t* inputBuffer,
        size_t inputBufferSize,
        C2FrameData::flags_t inputFrameFlag,
        uint64_t frame_index,
        uint64_t timestamp,
        C2BlockPool::local_id_t poolType);

    /* Queue buffer with fd (zero-copy) */
    c2_status_t queue (
        uint32_t fd,
        C2FrameData::flags_t inputFrameFlag,
        uint64_t frame_index,
        uint64_t timestamp,
        C2BlockPool::local_id_t poolType);

    c2_status_t flush (C2Component::flush_mode_t mode, std::list< std::unique_ptr< C2Work >> *const flushedWork); 
    c2_status_t drain (C2Component::drain_mode_t mode);
    c2_status_t start ();
    c2_status_t stop ();
    c2_status_t reset ();
    c2_status_t release ();
    C2ComponentInterfaceAdapter* intf ();
    c2_status_t createBlockpool(C2BlockPool::local_id_t poolType);
    c2_status_t setPoolProperty(C2BlockPool::local_id_t poolType, uint32_t width, uint32_t height, uint32_t fmt);

    /* Methods implementing Listener */
    void handleWorkDone(std::weak_ptr<C2Component> component, std::list<std::unique_ptr<C2Work>> workItems);
    void handleTripped(std::weak_ptr<C2Component> component, std::vector<std::shared_ptr<C2SettingResult>> settingResult);
    void handleError(std::weak_ptr<C2Component> component, uint32_t errorCode);

    /* This class methods */
    c2_status_t setListenercallback (std::unique_ptr<EventCallback> callback, c2_blocking_t mayBlock);
    c2_status_t setCompStore (std::weak_ptr<C2ComponentStore> store);
    c2_status_t freeOutputBuffer (uint64_t bufferIdx);
    c2_status_t setMapBufferToCpu (bool enable);

private:
    c2_status_t prepareC2Buffer(
        uint8_t* rawBuffer,
        uint32_t bufferSize,
        std::shared_ptr<QC2Buffer>* c2Buf,
        C2BlockPool::local_id_t poolType);

    c2_status_t writePlane(std::shared_ptr<QC2Buffer> buf, uint8_t *src);

    c2_status_t waitForProgressOrStateChange(
        uint32_t numPendingWorks,
        uint32_t timeoutMs);

    std::weak_ptr<C2ComponentStore> mStore;
    std::shared_ptr<C2Component> mComp;
    std::shared_ptr<C2ComponentInterfaceAdapter> mIntf;
    std::shared_ptr<C2Component::Listener> mListener;
    std::unique_ptr<EventCallback> mCallback;
    bool mMapBufferToCpu;

    std::shared_ptr<QC2LinearBufferPool> mLinearPool;
    std::shared_ptr<QC2GraphicBufferPool> mGraphicPool;
    std::map<uint64_t, std::shared_ptr<QC2Buffer>> mInPendingBuffer;
    std::map<uint64_t, std::shared_ptr<QC2Buffer>> mOutPendingBuffer;

    uint32_t mNumPendingWorks;
    std::mutex mLock;
    std::condition_variable mCondition;
};

} // namespace QTI

#endif /* __C2COMPONENTADAPTER_H__ */