// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Plugin.Module.h"
#include "Plugin.Module.g.cpp"

using namespace winrt;
using namespace VideoPlayer::Plugin::implementation;
using namespace Windows::Foundation;

_Use_decl_annotations_
void Module::Shutdown()
{
	std::lock_guard<slim_mutex> guard(m_mutex);

    m_deviceResources.reset();
	m_d3d11DeviceResources.reset();
}

_Use_decl_annotations_
void Module::OnRenderEvent(
    uint16_t frameNumber)
{
	std::shared_lock<slim_mutex> slock(m_mutex);

    UNREFERENCED_PARAMETER(frameNumber);
}

_Use_decl_annotations_
HRESULT Module::Initialize(
    std::weak_ptr<IUnityDeviceResource> const& unityDevice,
    StateChangedCallback stateCallback, 
    void* pCallbackObject)
{
	std::lock_guard<slim_mutex> guard(m_mutex);

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
HRESULT Module::Callback(
    CALLBACK_STATE state)
{
	std::shared_lock<slim_mutex> slock(m_mutex);

    NULL_CHK_HR(m_stateCallbacks, S_OK);

    m_stateCallbacks(m_pClientObject, state);

    return S_OK;
}
