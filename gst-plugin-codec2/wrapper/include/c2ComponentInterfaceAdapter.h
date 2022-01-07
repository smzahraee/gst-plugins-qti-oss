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

#ifndef __C2COMPONENTINTERFACEADAPTER_H__
#define __C2COMPONENTINTERFACEADAPTER_H__

#include "types.h"

#include <C2Component.h>

namespace QTI {

class C2ComponentInterfaceAdapter {

public:
    C2ComponentInterfaceAdapter(std::shared_ptr<C2ComponentInterface> compIntf);
    ~C2ComponentInterfaceAdapter();

    C2String getName() const;
    c2_node_id_t getId() const;

    // Apply configurations
    c2_status_t config(const std::vector<C2Param*>& stackParams, c2_blocking_t mayBlock);

private:
    // Set underlying Component
    c2_status_t setComponent(std::weak_ptr<C2Component> comp);

    // Underlying Component Interface
    std::shared_ptr<C2ComponentInterface> mCompIntf;

    // backing component (may be empty)
    std::weak_ptr<C2Component> mConnectedComponent;
};

} // namespace QTI

#endif /* __C2COMPONENTINTERFACEADAPTER_H__ */
