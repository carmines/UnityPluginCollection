// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Plugin.Module.g.h"
#include "D3D11DeviceResources.h"

struct __declspec(uuid("34fe2ecf-68d3-4732-a05a-2a737b63c386")) IModulePriv : ::IUnknown
{
    STDMETHOD(Initialize)(_In_ std::weak_ptr<IUnityDeviceResource> const& unityDevice, _In_ StateChangedCallback stateCallback, _In_ void* pCallbackObject) PURE;
    STDMETHOD(Callback)(_In_ CALLBACK_STATE state) PURE;
};

namespace winrt::VideoPlayer::Plugin::implementation
{
    struct Module : ModuleT<Module, IModulePriv>
    {
        Module() = default;

		void Shutdown();
		void OnRenderEvent(uint16_t frameNumber);

        // IModulePriv
        STDOVERRIDEMETHODIMP Initialize(_In_ std::weak_ptr<IUnityDeviceResource> const& unityDevice, _In_ StateChangedCallback stateCallback, _In_ void* pCallbackObject);
        STDOVERRIDEMETHODIMP Callback(_In_ CALLBACK_STATE state);

    protected:
        std::weak_ptr<IUnityDeviceResource> m_deviceResources;
        std::weak_ptr<ID3D11DeviceResource> m_d3d11DeviceResources;

    private:
        slim_mutex m_mutex;
        void* m_pClientObject;
        StateChangedCallback m_stateCallbacks;
    };
}
