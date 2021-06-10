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

#ifndef __C2COMPONENTSTOREADAPTER_H__
#define __C2COMPONENTSTOREADAPTER_H__

#include <C2Component.h>

namespace QTI {

  struct QC2ComponentStoreFactory {
    virtual ~QC2ComponentStoreFactory() = default;
    virtual std::shared_ptr<C2ComponentStore> getInstance() = 0;
  };

  // symbol name for getting the factory (library = libqcodec2_core.so)
  static constexpr const char * kFn_QC2ComponentStoreFactoryGetter = "QC2ComponentStoreFactoryGetter";

  using QC2ComponentStoreFactoryGetter_t
    = QC2ComponentStoreFactory * (*)(int majorVersion, int minorVersion);

class C2ComponentStoreAdapter {

public:

    C2ComponentStoreAdapter(std::shared_ptr<C2ComponentStore> store,
        QC2ComponentStoreFactory* factory, void* dl_handle);
    ~C2ComponentStoreAdapter();

    c2_status_t createComponent (C2String name, void **const component);
    c2_status_t createInterface (C2String name, void **const interface);
    C2String getName();
    std::vector<std::shared_ptr<const C2Component::Traits >> listComponents();
    bool isComponentSupported(char* name);

private:
    std::shared_ptr<C2ComponentStore> mStore;
    QC2ComponentStoreFactory* mFactory;
    void* mDlHandle;
};

} // namespace QTI

#endif /* __C2COMPONENTSTOREADAPTER_H__ */
