// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Plugin.Module.g.h"
#include "D3D11DeviceResources.h"

struct __declspec(uuid("34fe2ecf-68d3-4732-a05a-2a737b63c386")) IModulePriv : ::IUnknown
{
    virtual winrt::hresult __stdcall Initialize(_In_ std::weak_ptr<IUnityDeviceResource> const& unityDevice, _In_ StateChangedCallback stateCallback, _In_ void* pCallbackObject) = 0;
    virtual winrt::hresult __stdcall Callback(_In_ CALLBACK_STATE state) = 0;
    virtual winrt::hresult __stdcall Failed() = 0;
};

namespace winrt::CameraCapture::Plugin::implementation
{
    struct Module : ModuleT<Module, IModulePriv>
    {
        Module() = default;

        virtual void Shutdown();
        virtual void OnRenderEvent(uint16_t frameNumber);

        // IModulePriv
        virtual hresult __stdcall Initialize(_In_ std::weak_ptr<IUnityDeviceResource> const& unityDevice, _In_ StateChangedCallback stateCallback, _In_ void* pCallbackObject) override;
        virtual hresult __stdcall Callback(_In_ CALLBACK_STATE state) override;
        virtual hresult __stdcall Failed() override;

    protected:
        std::weak_ptr<IUnityDeviceResource> m_deviceResources;
        std::weak_ptr<ID3D11DeviceResource> m_d3d11DeviceResources;

    private:
        CriticalSection m_cs;
        void* m_pClientObject;
        StateChangedCallback m_stateCallbacks;
    };
}
