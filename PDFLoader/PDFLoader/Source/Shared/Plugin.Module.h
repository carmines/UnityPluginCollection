// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Plugin/Module.g.h"
#include "D3D11DeviceResources.h"

struct __declspec(uuid("34fe2ecf-68d3-4732-a05a-2a737b63c386")) IModulePriv : ::IUnknown
{
    STDMETHOD(Initialize)(_In_ std::weak_ptr<IUnityDeviceResource> const& unityDevice, _In_ StateChangedCallback stateCallback) PURE;
    STDMETHOD(Callback)(_In_ CALLBACK_STATE state) PURE;
};

namespace winrt::PDFLoader::Plugin::implementation
{
    struct Module : ModuleT<Module, IModulePriv>
    {
        Module() = default;

        virtual void Shutdown();
        virtual void OnRenderEvent(uint16_t frameNumber);

        // IModulePriv
        STDOVERRIDEMETHODIMP Initialize(_In_ std::weak_ptr<IUnityDeviceResource> const& unityDevice, _In_ StateChangedCallback stateCallback);
        STDOVERRIDEMETHODIMP Callback(_In_ CALLBACK_STATE state);

    protected:
        std::weak_ptr<IUnityDeviceResource> m_deviceResources;

    private:
        slim_mutex m_mutex;
        StateChangedCallback m_stateCallbacks;
    };
}
