// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Plugin.Module.h"
#include "Plugin.Module.g.cpp"

using namespace winrt;
using namespace CameraCapture::Plugin::implementation;
using namespace Windows::Foundation;

_Use_decl_annotations_
void Module::Shutdown()
{
    auto gurad = m_cs.Guard();

    m_deviceResources.reset();
    m_d3d11DeviceResources.reset();
}

_Use_decl_annotations_
void Module::OnRenderEvent(
    uint16_t frameNumber)
{
    auto gurad = m_cs.Guard();

    UNREFERENCED_PARAMETER(frameNumber);
}

_Use_decl_annotations_
hresult Module::Initialize(
    std::weak_ptr<IUnityDeviceResource> const& unityDevice,
    StateChangedCallback stateCallback, 
    void* pCallbackObject)
{
    auto gurad = m_cs.Guard();

    NULL_CHK_HR(stateCallback, E_INVALIDARG);

    auto resources = unityDevice.lock();
    if (resources != nullptr)
    {
        m_pClientObject = pCallbackObject;
        m_stateCallbacks = stateCallback;
        m_deviceResources = unityDevice;
        m_d3d11DeviceResources = std::dynamic_pointer_cast<ID3D11DeviceResource>(resources);
    }

    return S_OK;
}

_Use_decl_annotations_
hresult Module::Callback(
    CALLBACK_STATE state)
{
    auto gurad = m_cs.Guard();

    NULL_CHK_HR(m_stateCallbacks, S_OK);

    m_stateCallbacks(m_pClientObject, state);

    return S_OK;
}

_Use_decl_annotations_
hresult Module::Failed()
{
    CALLBACK_STATE state{};
    ZeroMemory(&state, sizeof(CALLBACK_STATE));

    state.type = CallbackType::Failed;

    ZeroMemory(&state.value.failedState, sizeof(FAILED_STATE));
    state.value.failedState.hresult = E_UNEXPECTED;

    auto gurad = m_cs.Guard();

    NULL_CHK_HR(m_stateCallbacks, E_NOT_VALID_STATE);
    NULL_CHK_HR(m_pClientObject, E_NOT_VALID_STATE);

    m_stateCallbacks(m_pClientObject, state);

    return state.value.failedState.hresult;
}
