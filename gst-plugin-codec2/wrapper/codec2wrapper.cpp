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
#include "wrapper_utils.h"
#include <sys/mman.h>


#include <string.h>
#include <C2PlatformSupport.h>
#include <C2Buffer.h>
#include <gst/gst.h>
#include <C2AllocatorGBM.h>
// config for some vendor parameters
#include "QC2V4L2Config.h"
#include <media/msm_media_info.h>


GST_DEBUG_CATEGORY (gst_qticodec2wrapper_debug);
#define GST_CAT_DEFAULT gst_qticodec2wrapper_debug

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
std::unique_ptr<C2Param> setRotation (gpointer param);
std::unique_ptr<C2Param> setMirrorType (gpointer param);
std::unique_ptr<C2Param> setRateControl (gpointer param);
std::unique_ptr<C2Param> setOutputPictureOrderMode (gpointer param);
std::unique_ptr<C2Param> setDecLowLatency (gpointer param);
std::unique_ptr<C2Param> setDownscale (gpointer param);
std::unique_ptr<C2Param> setEncColorSpaceConv (gpointer param);
std::unique_ptr<C2Param> setColorAspectsInfo (gpointer param);
std::unique_ptr<C2Param> setIntraRefresh (gpointer param);
std::unique_ptr<C2Param> setSliceMode (gpointer param);

// Function map for parameter configuration
static configFunctionMap sConfigFunctionMap = {
    {CONFIG_FUNCTION_KEY_PIXELFORMAT, setVideoPixelformat},
    {CONFIG_FUNCTION_KEY_RESOLUTION, setVideoResolution},
    {CONFIG_FUNCTION_KEY_BITRATE, setVideoBitrate},
    {CONFIG_FUNCTION_KEY_ROTATION, setRotation},
    {CONFIG_FUNCTION_KEY_MIRROR, setMirrorType},
    {CONFIG_FUNCTION_KEY_RATECONTROL, setRateControl},
    {CONFIG_FUNCTION_KEY_OUTPUT_PICTURE_ORDER_MODE, setOutputPictureOrderMode},
    {CONFIG_FUNCTION_KEY_DEC_LOW_LATENCY, setDecLowLatency},
    {CONFIG_FUNCTION_KEY_DOWNSCALE, setDownscale},
    {CONFIG_FUNCTION_KEY_ENC_CSC, setEncColorSpaceConv},
    {CONFIG_FUNCTION_KEY_COLOR_ASPECTS_INFO, setColorAspectsInfo},
    {CONFIG_FUNCTION_KEY_INTRAREFRESH, setIntraRefresh},
    {CONFIG_FUNCTION_KEY_SLICE_MODE, setSliceMode},
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

std::unique_ptr<C2Param> setMirrorType (gpointer param) {
    if (param == NULL)
        return nullptr;

    ConfigParams* config = (ConfigParams*)param;

    if (config->isInput) {
        qc2::C2VideoMirrorTuning::input mirror;
        mirror.mirrorType = qc2::QCMirrorType(config->mirror.type);
        return C2Param::Copy(mirror);
    } else {
        LOG_WARNING("setMirrorType output not implemented");
    }

    return nullptr;
}

std::unique_ptr<C2Param> setRotation (gpointer param) {
    if (param == NULL)
        return nullptr;

    ConfigParams* config = (ConfigParams*)param;

    if (config->isInput) {
        qc2::C2VideoRotation::input rotation;
        rotation.angle = config->val.u32;
        return C2Param::Copy(rotation);
    } else {
        LOG_WARNING("setRotation output not implemented");
    }

    return nullptr;
}

std::unique_ptr<C2Param> setRateControl (gpointer param) {
    if (param == NULL)
        return nullptr;

    ConfigParams* config = (ConfigParams*)param;

    C2StreamBitrateModeTuning::output bitrateMode;
    bitrateMode.value = (C2Config::bitrate_mode_t) toC2RateControlMode(config->rcMode.type);
    return C2Param::Copy(bitrateMode);
}

std::unique_ptr<C2Param> setOutputPictureOrderMode (gpointer param) {
    if (param == NULL)
        return nullptr;

    ConfigParams* config = (ConfigParams*)param;

    qc2::C2VideoPictureOrder::output outputPictureOrderMode;
    if (config->output_picture_order_mode == DECODER_ORDER)
        outputPictureOrderMode.enable = C2_TRUE;
    return C2Param::Copy(outputPictureOrderMode);
}

std::unique_ptr<C2Param> setSliceMode (gpointer param) {
    if (param == NULL)
        return nullptr;

    ConfigParams* config = (ConfigParams*)param;
    if (config->SliceMode.type == SLICE_MODE_BYTES) {
        qc2::C2VideoSliceSizeBytes::output SliceModeBytes;
        SliceModeBytes.value = config->val.u32;
        return C2Param::Copy(SliceModeBytes);
    } else if (config->SliceMode.type == SLICE_MODE_MB) {
        qc2::C2VideoSliceSizeMBCount::output SliceModeMb;
        SliceModeMb.value = config->val.u32;
        return C2Param::Copy(SliceModeMb);
    } else {
        return nullptr;
    }
}

std::unique_ptr<C2Param> setDecLowLatency (gpointer param) {
    if (param == NULL)
        return nullptr;

    ConfigParams* config = (ConfigParams*)param;

    C2GlobalLowLatencyModeTuning lowLatencyMode;
    lowLatencyMode.value = C2_TRUE;

    return C2Param::Copy(lowLatencyMode);
}

std::unique_ptr<C2Param> setDownscale (gpointer param) {

    if (param == NULL) {
        return nullptr;
    }

    ConfigParams* config = (ConfigParams*)param;

    if (config->isInput) {
      LOG_WARNING("setDownscale input not implemented");
    } else {
      qc2::C2VideoDownScalarSetting::output scale;

      scale.width = config->resolution.width;
      scale.height = config->resolution.height;

      return C2Param::Copy(scale);
    }

    return nullptr;
}

std::unique_ptr<C2Param> setEncColorSpaceConv (gpointer param) {
    if (param == NULL)
        return nullptr;

    ConfigParams* config = (ConfigParams*)param;

    qc2::C2VideoCSC::input colorSpaceConv;
    colorSpaceConv.value = config->color_space_conversion;
    return C2Param::Copy(colorSpaceConv);
}

std::unique_ptr<C2Param> setColorAspectsInfo (gpointer param) {
    if (param == NULL)
        return nullptr;

    ConfigParams* config = (ConfigParams*)param;

    C2StreamColorAspectsInfo::input colorAspects;
    colorAspects.primaries  = toC2Primaries(config->colorAspects.primaries);
    colorAspects.transfer   = toC2TransferChar(config->colorAspects.transfer_char);
    colorAspects.matrix     = toC2Matrix(config->colorAspects.matrix);
    colorAspects.range      = toC2FullRange(config->colorAspects.full_range);
    return C2Param::Copy(colorAspects);
}

std::unique_ptr<C2Param> setIntraRefresh (gpointer param) {
    if (param == NULL)
        return nullptr;

    ConfigParams* config = (ConfigParams*)param;

    C2StreamIntraRefreshTuning::output intraRefreshMode;
    intraRefreshMode.mode = (C2Config::intra_refresh_mode_t)config->irMode.type;
    intraRefreshMode.period = config->irMode.intra_refresh_mbs;
    return C2Param::Copy(intraRefreshMode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CodecCallback API handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CodecCallback : public EventCallback {
public:
    CodecCallback (const void* handle, listener_cb cb);
    ~CodecCallback ();

    void onOutputBufferAvailable (
        const std::shared_ptr<C2Buffer> &buffer,
        uint64_t index,
        uint64_t timestamp,
        C2FrameData::flags_t flag) override;
    void onTripped(uint32_t errorCode) override;
    void onError(uint32_t errorCode) override;
    void setMapBufferToCpu (bool enable) override;
private:
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
    const std::shared_ptr<C2Buffer> &buffer,
    uint64_t index,
    uint64_t timestamp,
    C2FrameData::flags_t flag) {

    if (!mCallback) {
        LOG_MESSAGE("Callback not set in CodecCallback(%p)", this);
        return;
    }

    BufferDescriptor outBuf;
    memset (&outBuf, 0, sizeof(BufferDescriptor));

    if (buffer) {
        C2BufferData::type_t buf_type = buffer->data().type();
        outBuf.timestamp = timestamp;
        outBuf.index = index;
        outBuf.flag = toWrapperFlag(flag);

        if (buf_type == C2BufferData::GRAPHIC) {
            const C2ConstGraphicBlock graphic_block = buffer->data().graphicBlocks().front();
            outBuf.fd = graphic_block.handle()->data[0];
            outBuf.meta_fd = graphic_block.handle()->data[1];
            guint32 stride = 0;
            guint64 usage = 0;
            guint32 size = 0;
            guint32 format = 0;

            _UnwrapNativeCodec2GBMMetadata (graphic_block.handle(), &outBuf.width, &outBuf.height, &format, &usage, &stride, &size);

            outBuf.size = size;
            if (mMapBufferToCpu) {
                /* get valid size for NV12_UBWC format */
                if (format == GBM_FORMAT_NV12 && (usage & GBM_BO_USAGE_UBWC_ALIGNED_QTI)) {
                    outBuf.size = VENUS_BUFFER_SIZE_USED (COLOR_FMT_NV12_UBWC, outBuf.width, outBuf.height, 0);
                }
                C2GraphicView view(graphic_block.map().get());
                outBuf.data = (guint8 *)view.data()[0];
                /* graphic_block unmapped once out of scope. */
                mCallback(mHandle, EVENT_OUTPUTS_DONE, &outBuf);
            } else {
                outBuf.data = NULL;
                mCallback(mHandle, EVENT_OUTPUTS_DONE, &outBuf);
            }

            LOG_INFO("out buffer size:%d width:%d height:%d stride:%d data:%p\n",
                size, outBuf.width, outBuf.height, stride, outBuf.data);
        } else if (buf_type == C2BufferData::LINEAR) {
            const C2ConstLinearBlock linear_block = buffer->data().linearBlocks().front();
            C2ReadView view(linear_block.map().get());
            outBuf.size = linear_block.size();
            outBuf.fd = linear_block.handle()->data[0];
            outBuf.data = (guint8 *)view.data();
            LOG_INFO("outBuf linear data:%p fd:%d size:%d\n", outBuf.data, outBuf.fd, outBuf.size);
            /* Check for codec data */
            auto csd = std::static_pointer_cast<const C2StreamInitDataInfo::output>(
              buffer->getInfo(C2StreamInitDataInfo::output::PARAM_TYPE));
            if (csd) {
              LOG_INFO("get codec config data, size: %lu data:%p", csd->flexCount(), (guint8 *)csd->m.value);
              outBuf.config_data = (guint8 *)&csd->m.value;
              outBuf.config_size = csd->flexCount();
              outBuf.flag = FLAG_TYPE_CODEC_CONFIG;
            }
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

void CodecCallback::setMapBufferToCpu (bool enable) {

    mMapBufferToCpu = enable;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ComponentStore API handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* c2componentStore_create () {

    GST_DEBUG_CATEGORY_INIT (gst_qticodec2wrapper_debug,
      "qticodec2wrapper", 0, "QTI GST codec2.0 wrapper");

    LOG_MESSAGE("Creating component store");
    void *lib = dlopen("libqcodec2_core.so", RTLD_NOW);
    if (lib == nullptr) {
        LOG_ERROR("failed to open %s: %s", "libqcodec2_core.so", dlerror());
        return nullptr;
    }

    auto factoryGetter =
        (QC2ComponentStoreFactoryGetter_t)dlsym(lib, kFn_QC2ComponentStoreFactoryGetter);

    if (factoryGetter == nullptr) {
        LOG_ERROR("failed to load symbol %s: %s", kFn_QC2ComponentStoreFactoryGetter, dlerror());
        dlclose(lib);
        return nullptr;
    }

    auto c2StoreFactory = (*factoryGetter)(1, 0);    // get version 1.0
    if (c2StoreFactory == nullptr) {
        LOG_ERROR("failed to get Store factory !");
        dlclose(lib);
        return nullptr;
    }

    std::shared_ptr<C2ComponentStore> store = c2StoreFactory->getInstance();

    return new C2ComponentStoreAdapter(store, c2StoreFactory, lib);
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

gboolean c2componentStore_isComponentSupported (void* const comp_store, gchar* name) {
    gboolean ret = FALSE;

    if (comp_store) {
        C2ComponentStoreAdapter* store_Wrapper = (C2ComponentStoreAdapter*)comp_store;

        bool ret = store_Wrapper->isComponentSupported(name);
        if (ret == true)
          return TRUE;
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

gboolean c2component_alloc(void* const comp, BufferDescriptor* buffer) {

    LOG_MESSAGE("Comp %p allocate buffer type: %d", comp, buffer->pool_type);

    gboolean ret = FALSE;
    std::shared_ptr<C2Buffer> buf = NULL;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        buf = comp_wrapper->alloc(buffer);

        if (buf != NULL) {
            if (buffer->pool_type == BUFFER_POOL_BASIC_GRAPHIC) {
                ret = TRUE;
            } else {
                LOG_ERROR("Unsupported pool type: %d", buffer->pool_type);
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

        c2Status = comp_wrapper->queue(buffer);

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

gboolean c2component_configBlockpool (void* comp, BUFFER_POOL_TYPE poolType) {

    LOG_MESSAGE("Configing block pool");

    gboolean ret = FALSE;
    c2_status_t c2Status = C2_NO_INIT;

    if (comp) {
        C2ComponentAdapter* comp_wrapper = (C2ComponentAdapter*)comp;

        c2Status = comp_wrapper->configBlockPool(toC2BufferPoolType(poolType));
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

void _push_to_settings (gpointer data, gpointer user_data) {
    std::list<std::unique_ptr<C2Param>> *settings = (std::list<std::unique_ptr<C2Param>> *)user_data;
    ConfigParams *conf_param = (ConfigParams*) data;

    auto iter = sConfigFunctionMap.find(conf_param->config_name);
    if (iter != sConfigFunctionMap.end()) {
      auto param = (*iter->second)(conf_param);
      settings->push_back(C2Param::Copy(*param));
    }
}

gboolean c2componentInterface_config (void* const comp_intf, GPtrArray* config, BLOCK_MODE_TYPE block) {

    LOG_MESSAGE("Applying configuration");

    gboolean ret = FALSE;

    if (comp_intf && config) {
        C2ComponentInterfaceAdapter* intf_wrapper = (C2ComponentInterfaceAdapter*)comp_intf;
        std::vector<C2Param*> stackParams;
        std::list<std::unique_ptr<C2Param>> settings;
        c2_status_t c2Status = C2_NO_INIT;

        g_ptr_array_foreach (config, _push_to_settings, &settings);

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
