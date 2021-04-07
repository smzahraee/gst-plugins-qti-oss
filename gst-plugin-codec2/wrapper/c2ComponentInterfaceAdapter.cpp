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

#include "c2ComponentInterfaceAdapter.h"
#include <gst/gst.h>

GST_DEBUG_CATEGORY_EXTERN (gst_qticodec2wrapper_debug);
#define GST_CAT_DEFAULT gst_qticodec2wrapper_debug

namespace QTI {

C2ComponentInterfaceAdapter::C2ComponentInterfaceAdapter(std::shared_ptr<C2ComponentInterface> compIntf) {

    mCompIntf = std::move(compIntf);
}

C2ComponentInterfaceAdapter::~C2ComponentInterfaceAdapter() {
    LOG_MESSAGE("delete C2 Component Interface Adapter");
    mCompIntf = nullptr;
}

C2String C2ComponentInterfaceAdapter::getName () const  {

    return mCompIntf->getName();
}

c2_node_id_t C2ComponentInterfaceAdapter::getId () const  {

    return mCompIntf->getId();
}

c2_status_t C2ComponentInterfaceAdapter::config (const std::vector<C2Param*> &stackParams, c2_blocking_t mayBlock) {

    LOG_MESSAGE("Component interface (%p) configured", this);

    c2_status_t result = C2_NO_INIT;
    std::vector<std::unique_ptr<C2SettingResult>> failures;

    result = mCompIntf->config_vb(stackParams, mayBlock, &failures);
    if ((C2_OK != result) || (failures.size() != 0)) {
        LOG_WARNING("Configuration failed(%d)", static_cast<int32_t>(result));
    }

    return result;
}

c2_status_t C2ComponentInterfaceAdapter::setComponent(std::weak_ptr<C2Component> comp) {

    c2_status_t result = C2_NO_INIT;

    if (!comp.expired()){
        mConnectedComponent = comp;
        result = C2_OK;
    }

    return result;
}

} // namespace QTI
