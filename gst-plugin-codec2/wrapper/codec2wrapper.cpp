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

#include "c2ComponentStoreAdapter.h"
#include "c2ComponentAdapter.h"
#include "c2ComponentInterfaceAdapter.h"
#include "codec2wrapper.h"
#include "utils.h"

#include <QC2Platform.h>
#include <string.h>
#include <C2PlatformSupport.h>
#include <QC2ComponentStore.h>
#include <QC2Buffer.h>

using namespace QTI;

struct char_cmp {
    bool operator () (const char *a,const char *b) const {
        return strcmp(a,b)<0;
    }
};

// Give a comparison functor to the map to avoid comparing the pointer
typedef std::map<const char*, configFunction, char_cmp> configFunctionMap;

std::unique_ptr<C2Param> setVideoPixelformat (gpointer param);
std::unique_ptr<C2Param> setVideoResolution (gpointer param);
std::unique_ptr<C2Param> setVideoBitrate (gpointer param);
std::unique_ptr<C2Param> setVideoInterlaceMode (gpointer param);

// Function map for parameter configuration
static configFunctionMap sConfigFunctionMap = {
    {CONFIG_FUNCTION_KEY_PIXELFORMAT, setVideoPixelformat},
    {CONFIG_FUNCTION_KEY_RESOLUTION, setVideoResolution},
    {CONFIG_FUNCTION_KEY_BITRATE, setVideoBitrate},
    {CONFIG_FUNCTION_KEY_INTERLACE, setVideoInterlaceMode}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parameter Builder
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<C2Param> setVideoPixelformat (gpointer param) {

    if (param == NULL) {
        return nullptr;
    }

    ConfigParams* config = (ConfigParams*)param;

    if (config->isInput) {
        C2StreamPixelFormatInfo::input inputColorFmt;

        inputColorFmt.value = toC2PixelFormat(config->pixelFormat.fmt);

        return C2Param::Copy(inputColorFmt);
    } else {
        C2StreamPixelFormatInfo::output outputColorFmt;

        outputColorFmt.value = toC2PixelFormat(config->pixelFormat.fmt);

        return C2Param::Copy(outputColorFmt);
    }
}

std::unique_ptr<C2Param> setVideoResolution (gpointer param) {

    if (param == NULL) {
        return nullptr;
    }

    ConfigParams* config = (ConfigParams*)param;

    if (config->isInput) {
        C2StreamPictureSizeInfo::input size;

        size.width = config->resolution.width;
        size.height = config->resolution.height;

        return C2Param::Copy(size);
    } else {
        C2StreamPictureSizeInfo::output size;

        size.width = config->resolution.width;
        size.height = config->resolution.height;

        return C2Param::Copy(size);
    }
}

std::unique_ptr<C2Param> setVideoBitrate (gpointer param) {

    if (param == NULL) {
        return nullptr;
    }

    ConfigParams* config = (ConfigParams*)param;

    if (config->isInput) {
        LOG_WARNING("setVideoBitrate input not implemented");

    } else {
        C2StreamBitrateInfo::output bitrate;

        bitrate.value = config->val.u32;

        return C2Param::Copy(bitrate);
    }

    return nullptr;
}

std::unique_ptr<C2Param> setVideoInterlaceMode (gpointer param) {

    if (param == NULL) {
        return nullptr;
    }

    ConfigParams* config = (ConfigParams*)param;

    if (config->isInput) {
        C2VideoInterlaceInfo::input interlaceMode;

        interlaceMode.format = toC2InterlaceType(config->interlaceMode.type);

        return C2Param::Copy(interlaceMode);

    } else {
        C2VideoInterlaceInfo::output interlaceMode;

        interlaceMode.format = toC2InterlaceType(config->interlaceMode.type);

        return C2Param::Copy(interlaceMode);
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CodecCallback API handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CodecCallback : public EventCallback {
public:
    CodecCallback (const void* handle, listener_cb cb);
    ~CodecCallback ();

    void onOutputBufferAvailable (
        const std::shared_ptr<QC2Buffer> &buffer,
        uint64_t index,
        uint64_t timestamp,
        C2FrameData::flags_t flag) override;
    void onTripped(uint32_t errorCode) override;
    void onError(uint32_t errorCode) override;
    void setMapBufferToCpu (bool enable) override;
private:
    int32_t getBufferFD (const std::shared_ptr<QC2Buffer> &buffer);
    int32_t getBufferMetaFD (const std::shared_ptr<QC2Buffer> &buffer);
    uint32_t getBufferCapacity (const std::shared_ptr<QC2Buffer> &buffer);
    uint32_t getBufferSize (const std::shared_ptr<QC2Buffer> &buffer);
    uint32_t getBufferOffset (const std::shared_ptr<QC2Buffer> &buffer);

    listener_cb mCallback;
    const void* mHandle;
    bool mMapBufferToCpu;
};

CodecCallback::CodecCallback (const void* handle, listener_cb cb) {

    LOG_MESSAGE("CodecCallback(%p) created", this);

    mCallback = cb;
    mHandle = handle;
    mMapBufferToCpu = false;
}

CodecCallback::~CodecCallback () {

    LOG_MESSAGE("CodecCallback(%p) destroyed", this);
}

void CodecCallback::onOutputBufferAvailable (
    const std::shared_ptr<QC2Buffer> &buffer,
    uint64_t index,
    uint64_t timestamp,
    C2FrameData::flags_t flag) {

    if (!mCallback) {
        LOG_MESSAGE("Callback not set in CodecCallback(%p)", this);
        return;
    }

    BufferDescriptor outBuf;

    if (buffer) {
        outBuf.fd = getBufferFD(buffer);
        outBuf.size = getBufferSize(buffer);
        outBuf.capacity = getBufferCapacity(buffer);
        outBuf.offset = getBufferOffset(buffer);
        outBuf.timestamp = timestamp;
        outBuf.index = index;
        outBuf.flag = toWrapperFlag(flag);

        if (buffer->isGraphic()) {
            outBuf.meta_fd = getBufferMetaFD(buffer);
            if (mMapBufferToCpu) {
                auto& g = buffer->graphic();
                auto map = g.mapReadOnly();
                outBuf.data = (guint8 *)map->base();
            }
            else {
                outBuf.data = NULL;
            }
            mCallback(mHandle, EVENT_OUTPUTS_DONE, &outBuf);
        } else if (buffer->isLinear()) {
            /* Check for codec data */
            auto& infos = buffer->infos();
            for (auto& info : infos) {
                if (info && info->coreIndex().coreIndex() ==
                    C2StreamInitDataInfo::output::CORE_INDEX) {
                    BufferDescriptor codecConfigBuf;
                    auto csd = (C2StreamInitDataInfo::output*)info.get();

                    LOG_INFO("get codec config data, size: %lu", csd->flexCount());
                    codecConfigBuf.data = csd->m.value;
                    codecConfigBuf.size = csd->flexCount();
                    codecConfigBuf.timestamp = 0;
                    codecConfigBuf.fd = -1;
                    codecConfigBuf.meta_fd = -1;
                    codecConfigBuf.capacity = 0;
                    codecConfigBuf.offset = 0;
                    codecConfigBuf.index = 0;
                    codecConfigBuf.flag = FLAG_TYPE_CODEC_CONFIG;
                    mCallback(mHandle, EVENT_OUTPUTS_DONE, &codecConfigBuf);
                }
            }

            /* Always map output buffer for linear output */
            auto& l = buffer->linear();
            auto map = l.mapReadOnly();
            outBuf.data = (guint8 *)map->base();
            mCallback(mHandle, EVENT_OUTPUTS_DONE, &outBuf);
        }
    }
    else if (flag & C2FrameData::FLAG_END_OF_STREAM) {
        LOG_MESSAGE("Mark EOS buffer");
        outBuf.data = NULL;
        outBuf.fd = -1;
        outBuf.meta_fd = -1;
        outBuf.size = 0;
        outBuf.capacity = 0;
        outBuf.offset = 0;
        outBuf.timestamp = 0;
        outBuf.index = 0;
        outBuf.flag = toWrapperFlag(flag);

        mCallback(mHandle, EVENT_OUTPUTS_DONE, &outBuf);
     }
    else {
        LOG_MESSAGE("Buffer is null");
    }
}

void CodecCallback::onTripped(uint32_t errorCode) {

    if (!mCallback) {
        LOG_MESSAGE("Callback not set in CodecCallback(%p)", this);
        return;
    }

    mCallback(mHandle, EVENT_TRIPPED, &errorCode);
}

void CodecCallback::onError(uint32_t errorCode) {

    if (!mCallback) {
        LOG_MESSAGE("Callback not set in CodecCallback(%p)", this);
        return;
    }

    mCallback(mHandle, EVENT_ERROR, &errorCode);
}

int32_t CodecCallback::getBufferFD(const std::shared_ptr<QC2Buffer> &buffer) {

    int32_t fd = 0;
    if (buffer->isGraphic()) {
        fd = buffer->graphic().fd();
    }
    else if (buffer->isLinear()) {
        fd = buffer->linear().fd();
    }

    return fd;
}

int32_t CodecCallback::getBufferMetaFD(const std::shared_ptr<QC2Buffer> &buffer) {

    int32_t meta_fd = -1;

    if (buffer->isGraphic()) {
        meta_fd = buffer->graphic().meta_fd();
    }
    else {
        LOG_ERROR("Meta fd only supported for graphic buffer");
    }

    return meta_fd;
}


uint32_t CodecCallback::getBufferCapacity (const std::shared_ptr<QC2Buffer> &buffer) {

    uint32_t capacity = 0;

    if (buffer->isGraphic()) {
        capacity = buffer->graphic().allocSize();
    }
    else if (buffer->isLinear()) {
        capacity = buffer->linear().capacity();
    }

    return capacity;
}

uint32_t CodecCallback::getBufferSize (const std::shared_ptr<QC2Buffer> &buffer) {

    uint32_t size = 0;

    if (buffer->isGraphic()) {
        size = buffer->graphic().allocSize();
    }
    else if (buffer->isLinear()) {
        size = buffer->linear().size();
    }

    return size;
}

uint32_t CodecCallback::getBufferOffset (const std::shared_ptr<QC2Buffer> &buffer) {

    uint32_t offset = 0;

    if (buffer->isGraphic()) {
        offset = buffer->graphic().offset();
    }
    else if (buffer->isLinear()) {
        offset = buffer->linear().offset();
    }

    return offset;
}

void CodecCallback::setMapBufferToCpu (bool enable) {

    mMapBufferToCpu = enable;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ComponentStore API handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* c2componentStore_create () {

    LOG_MESSAGE("Creating component store");

    std::shared_ptr<C2ComponentStore> store = qc2::QC2ComponentStore::Get();
    if (store == NULL) {
        LOG_ERROR("Failed to create component store");
    }

    return new C2ComponentStoreAdapter(store);
}

const gchar* c2componentStore_getName (void* const comp_store) {

    gchar* name = NULL;

    if (comp_store) {
        C2ComponentStoreAdapter* store_Wrapper = (C2ComponentStoreAdapter*)comp_store;
        name = g_strdup(store_Wrapper->getName().c_str());
    } else{
        LOG_ERROR("Component store is null");
    }

    return name;
}

gboolean c2componentStore_createComponent (void* const comp_store, const gchar* name, void** const component) {

    LOG_MESSAGE("Creating component");

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp_store) {
        C2ComponentStoreAdapter* store_Wrapper = (C2ComponentStoreAdapter*)comp_store;

        c2Status = store_Wrapper->createComponent(C2String(name), component);
        if (c2Status == C2_OK) {
            ret = TRUE;
        } else {
            LOG_ERROR("Failed(%d) to create component (%s)", c2Status, name);
        }
    } else{
        LOG_ERROR("Component store is null");
    }

    return ret;
}

gboolean c2componentStore_createInterface (void* const comp_store, const gchar* name, void** const interface) {

    LOG_MESSAGE("Creating component interface");

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp_store) {
        C2ComponentStoreAdapter* store_Wrapper = (C2ComponentStoreAdapter*)comp_store;

        c2Status = store_Wrapper->createInterface(C2String(name), interface);
        if (c2Status == C2_OK) {
            ret = TRUE;
        } else {
            LOG_ERROR("Failed(%d) to create component interface (%s)", c2Status, name);
        }
    } else {
        LOG_ERROR("Component store is null");
    }

    return ret;
}

gboolean c2componentStore_listComponents (void* const comp_store, GPtrArray* array) {

    gboolean ret = FALSE;

    if (comp_store) {
        C2ComponentStoreAdapter* store_Wrapper = (C2ComponentStoreAdapter*)comp_store;

        std::vector<std::shared_ptr<const C2Component::Traits>> components = store_Wrapper->listComponents();
        for (auto component : components) {
            g_ptr_array_add (array, (gpointer)component->name.c_str());
        }
        ret = true;
    }

    return ret;
}

gboolean c2componentStore_delete(void* comp_store){

    LOG_MESSAGE("Deleting component store");

    gboolean ret = FALSE;

    if (comp_store) {
        C2ComponentStoreAdapter* store_Wrapper = (C2ComponentStoreAdapter*)comp_store;

        delete store_Wrapper;
        ret = TRUE;
    }

    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Component API handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
gboolean c2component_setListener(void* const comp, void* cb_context, listener_cb listener, BLOCK_MODE_TYPE mayBlock) {

    LOG_MESSAGE("Updating component listener");

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;
        std::unique_ptr<EventCallback> callback = std::make_unique<CodecCallback>(cb_context, listener);

        c2Status = comp_wrapper->setListenercallback(std::move(callback), toC2BlocingType(mayBlock));
        if (c2Status == C2_OK) {
            ret = TRUE;
        } else {
            LOG_ERROR("Failed(%d) to set component listener is null", c2Status);
        }
    } else {
        LOG_ERROR("Component is null");
    }

    return ret;
}

gboolean c2component_alloc(void* const comp, BufferDescriptor* buffer, BUFFER_POOL_TYPE poolType) {

    LOG_MESSAGE("Comp %p allocate buffer type: %d", comp, poolType);

    gboolean ret = FALSE;
    C2BlockPool::local_id_t type = toC2BufferPoolType(poolType);
    std::shared_ptr<QC2Buffer> buf = NULL;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        /* When callling into alloc(), it's assuming that the allocation
         * parms(resolution/size, format) are already passed into allocator.
         * This can be done by calling c2component_set_pool_property() */
        buf = comp_wrapper->alloc(type);

        if (buf != NULL) {
            if (poolType == BUFFER_POOL_BASIC_GRAPHIC) {
                auto& g = buf->graphic();
                buffer->fd = g.fd();
                buffer->capacity = g.allocSize();

                ret = TRUE;
            } else {
                LOG_ERROR("Unsupported pool type: %d", poolType);
            }
        } else {
            LOG_ERROR("Failed to alloc buffer");
        }
    } else {
        LOG_ERROR("Component is null");
    }

    return ret;
}

gboolean c2component_queue(void* const comp, BufferDescriptor* buffer) {

    LOG_MESSAGE("Queueing work");

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        /* check if input buffer contains fd/va and decide if we need to
         * allocate a new C2 buffer or not */
        if (buffer->fd > 0) {
          c2Status = comp_wrapper->queue(
              buffer->fd,
              toC2Flag(buffer->flag),
              buffer->index,
              buffer->timestamp,
              toC2BufferPoolType(buffer->pool_type));
        } else {
          c2Status = comp_wrapper->queue(
              buffer->data,
              buffer->size,
              toC2Flag(buffer->flag),
              buffer->index,
              buffer->timestamp,
              toC2BufferPoolType(buffer->pool_type));
        }
        if (c2Status == C2_OK) {
            ret = TRUE;
        } else {
            LOG_ERROR("Failed to queue work (%d)", c2Status);
        }
    } else {
        LOG_ERROR("Component is null");
    }

    return ret;
}

gboolean c2component_flush (void* const comp, FLUSH_MODE_TYPE mode, void* const flushedWork) {

    LOG_MESSAGE("Flushing work");

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        LOG_MESSAGE("Not implemented");
    }

    return ret;
}

gboolean c2component_drain (void* const comp, DRAIN_MODE_TYPE mode) {

    LOG_MESSAGE("Draining work");

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        LOG_MESSAGE("Not implemented");
    }

    return ret;
}

gboolean c2component_start (void* const comp) {

    LOG_MESSAGE("Starting component");

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        c2Status = comp_wrapper->start();
        if (c2Status == C2_OK) {
            ret = TRUE;
        } else {
            LOG_ERROR("Failed(%d) to start component", c2Status);
        }
    } else {
        LOG_ERROR("Component is null");
    }

    return ret;
}

gboolean c2component_stop (void* const comp) {

    LOG_MESSAGE("Stopping component");

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp) {

        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;
        c2Status = comp_wrapper->stop();
        if (c2Status == C2_OK) {
            ret = TRUE;
        } else {
            LOG_ERROR("Failed(%d) to stop component", c2Status);
        }
    } else {
        LOG_ERROR("Component is null");
    }

    return ret;
}

gboolean c2component_reset (void* const comp) {

    LOG_MESSAGE("Resetting component");

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        c2Status = comp_wrapper->reset();
        if (c2Status == C2_OK) {
            ret = TRUE;
        } else {
            LOG_ERROR("Failed(%d) to reset component", c2Status);
        }
    } else {
        LOG_ERROR("Component is null");
    }

    return ret;
}

gboolean c2component_release (void* const comp) {

    LOG_MESSAGE("Releasing component");

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        c2Status = comp_wrapper->release();
        if (c2Status == C2_OK) {
            ret = TRUE;
        }  else {
            LOG_ERROR("Failed(%d) to release component", c2Status);
        }
    } else {
        LOG_ERROR("Component is null");
    }

    return ret;
}

void* c2component_intf (void* const comp) {

    LOG_MESSAGE("Creating component interface");

    void* compIntf = NULL;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        compIntf = comp_wrapper->intf();
    }  else {
        LOG_ERROR("Component is null");
    }

    if (compIntf == NULL) {
        LOG_ERROR("Component interface is null");
    }

    return compIntf;
}

gboolean c2component_createBlockpool(void* comp, BUFFER_POOL_TYPE poolType) {

    LOG_MESSAGE("Creating block pool");

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        c2Status = comp_wrapper->createBlockpool(toC2BufferPoolType(poolType));
        if (c2Status == C2_OK) {
            ret = TRUE;
        } else {
            LOG_ERROR("Failed(%d) to allocate block pool(%d)", c2Status, poolType);
        }
    }

    return ret;
}

gboolean c2component_mapOutBuffer (void* const comp, gboolean map) {

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        c2Status = comp_wrapper->setMapBufferToCpu((map == TRUE) ? true:false);
        if (c2Status == C2_OK) {
            ret = TRUE;
        }
    }

    return ret;
}

gboolean c2component_freeOutBuffer (void* const comp, guint64 bufferId) {

    LOG_MESSAGE("Freeing buffer");

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        c2Status = comp_wrapper->freeOutputBuffer(bufferId);
        if (c2Status == C2_OK) {
            ret = TRUE;
        } else {
            LOG_ERROR("Failed(%d) to free buffer(%lu)", c2Status, bufferId);
        }
    }

    return ret;
}

gboolean c2component_delete(void* comp) {

    LOG_MESSAGE("Deleting component");

    gboolean ret = FALSE;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;
        delete comp_wrapper;
        ret = TRUE;
    }
    return ret;
}

gboolean c2component_set_pool_property (void* comp, BUFFER_POOL_TYPE poolType, guint32 width,
                                        guint32 height, PIXEL_FORMAT_TYPE fmt) {
    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        c2Status = comp_wrapper->setPoolProperty(toC2BufferPoolType(poolType), width,
                                                 height, toC2PixelFormat(fmt));
        if (c2Status == C2_OK) {
            ret = TRUE;
        } else {
            LOG_ERROR("Failed(%d) to set pool property", c2Status);
        }
    } else {
        LOG_ERROR("Fail to set pool property. comp is NULL");
    }

    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ComponentInterface API handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const gchar* c2componentInterface_getName(void* const comp_intf) {

    gchar* name = NULL;

    if (comp_intf) {
        C2ComponentInterfaceAdapter* intf_wrapper = (C2ComponentInterfaceAdapter*)comp_intf;

        name = g_strdup(intf_wrapper->getName().c_str());
    }

    return name;
}

const gint  c2componentInterface_getId(void* const comp_intf) {

    gint ret = -1;
    if (comp_intf) {
        C2ComponentInterfaceAdapter* intf_wrapper = (C2ComponentInterfaceAdapter*)comp_intf;

        ret = intf_wrapper->getId();
    }

    return ret;
}

gboolean c2componentInterface_config (void* const comp_intf, GHashTable* config, BLOCK_MODE_TYPE block) {

    LOG_MESSAGE("Applying configuration");

    gboolean ret = FALSE;

    if (comp_intf && config) {
        C2ComponentInterfaceAdapter* intf_wrapper = (C2ComponentInterfaceAdapter*)comp_intf;
        std::vector<C2Param*> stackParams;
        std::list<std::unique_ptr<C2Param>> settings;
        c2_status_t c2Status = C2_NO_INIT;
        GHashTableIter iter;
        gpointer key;
        gpointer value;

        g_hash_table_iter_init (&iter, config);

        while (g_hash_table_iter_next (&iter, &key, &value)) {
            auto iter = sConfigFunctionMap.find((const char*)key);
            if (iter != sConfigFunctionMap.end()) {
                auto param = (*iter->second)(value);
                settings.push_back(C2Param::Copy(*param));
            }
        }

        for (auto &item: settings) {
          stackParams.push_back(item.get());
        }

        c2Status = intf_wrapper->config(stackParams, toC2BlocingType(block));
        if (c2Status == C2_OK) {
            ret = TRUE;
        } else{
            LOG_WARNING("Failed(%d) to apply the configuration", c2Status);
        }
    }

    return ret;
}

gboolean c2componentInterface_delete(void* comp_intf) {

    LOG_MESSAGE("Deleting component interface");

    gboolean ret = FALSE;

    if (comp_intf) {
        C2ComponentInterfaceAdapter* intf_wrapper = (C2ComponentInterfaceAdapter*)comp_intf;

        delete intf_wrapper;
        ret = TRUE;
    }

    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
guint32 get_output_frame_size(guint32 width, guint32 height, PIXEL_FORMAT_TYPE fmt) {

    guint32 size = 0;
    guint32 pixel_fmt = toC2PixelFormat(fmt);

    if (PixFormat::IsCompressed(pixel_fmt)) {
        size = Platform::VenusBufferLayout::CompressedFrameSize(pixel_fmt, width, height);
    } else {
        //TODO: Support frame size calculation for uncompressed formats
    }

    LOG_MESSAGE("Frame size: %d for color format: %d, width: %d, height: %d", size, pixel_fmt, width, height);

    return size;
}
