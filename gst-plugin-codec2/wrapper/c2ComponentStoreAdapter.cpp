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
#include <gst/gst.h>

GST_DEBUG_CATEGORY_EXTERN (gst_qticodec2wrapper_debug);
#define GST_CAT_DEFAULT gst_qticodec2wrapper_debug

namespace QTI {

C2ComponentStoreAdapter::C2ComponentStoreAdapter(std::shared_ptr<C2ComponentStore> store) {

    mStore = std::move(store);
}

C2ComponentStoreAdapter::~C2ComponentStoreAdapter() {

    mStore = nullptr;
}

C2String C2ComponentStoreAdapter::getName() {

    C2String name;

    if (mStore) {
        name = mStore->getName();
    }

    return name;
}

c2_status_t C2ComponentStoreAdapter::createComponent(C2String name, void **const component) {

    c2_status_t result = C2_BAD_VALUE;
    std::shared_ptr<C2Component> comp = nullptr;

    result = mStore->createComponent(name, &comp);
    if ((result == C2_OK) && (comp != nullptr)) {
        C2ComponentAdapter* comp_adapter = new C2ComponentAdapter(comp);

        if (comp_adapter) {
            *component = comp_adapter;
        }
    }

    return result;
}

c2_status_t C2ComponentStoreAdapter::createInterface (C2String name, void **const interface) {

    c2_status_t result = C2_BAD_VALUE;
    std::shared_ptr<C2ComponentInterface> compIntf = nullptr;

    result = mStore->createInterface(name, &compIntf);
    if (result == C2_OK) {

        C2ComponentInterfaceAdapter* intf_adapter = new C2ComponentInterfaceAdapter(compIntf);
        if (intf_adapter != nullptr) {
 
            *interface = intf_adapter;
        }
    }

    return result;
}

std::vector<std::shared_ptr<const C2Component::Traits>> C2ComponentStoreAdapter::listComponents() {

    std::vector<std::shared_ptr<const C2Component::Traits>> result;

    if (mStore) {
        result = mStore->listComponents();
    }

    return result;
}

} // namespace QTI
