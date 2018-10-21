// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Plugin.Module.h"

using namespace winrt;
using namespace PDFLoader::Plugin::implementation;
using namespace Windows::Foundation;

_Use_decl_annotations_
void Module::Shutdown()
{
    auto guard = slim_lock_guard(m_mutex);

    m_deviceResources.reset();
}

_Use_decl_annotations_
void Module::OnRenderEvent(
    uint16_t frameNumber)
{
    auto guard = slim_shared_lock_guard(m_mutex);

    UNREFERENCED_PARAMETER(frameNumber);
}

_Use_decl_annotations_
HRESULT Module::Initialize(
    std::weak_ptr<IUnityDeviceResource> const& unityDevice,
    StateChangedCallback stateCallback)
{
    auto guard = slim_lock_guard(m_mutex);

    NULL_CHK_HR(stateCallback, E_INVALIDARG);

    auto resources = unityDevice.lock();
    if (resources != nullptr)
    {
        com_ptr<ID3D11DeviceResource> spD3D11Resources = nullptr;
        IFR(resources->QueryInterface(__uuidof(ID3D11DeviceResource), spD3D11Resources.put_void()));

        m_stateCallbacks = stateCallback;
        m_deviceResources = unityDevice;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT Module::Callback(
    CALLBACK_STATE state)
{
    auto guard = slim_lock_guard(m_mutex);

    NULL_CHK_HR(m_stateCallbacks, S_OK);

    m_stateCallbacks(state);

    return S_OK;
}
