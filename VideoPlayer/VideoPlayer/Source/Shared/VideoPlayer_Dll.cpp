// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "Unity/IUnityGraphics.h"
#include "PlatformBase.h"
#include "UnityDeviceResource.h"

#include "Plugin.PlaybackManager.h"

namespace impl
{
    using namespace winrt::VideoPlayer::Plugin::implementation;
}

namespace winrt
{
    using namespace winrt::VideoPlayer::Plugin;
}

static INSTANCE_HANDLE s_lastPluginHandleIndex = INSTANCE_HANDLE_INVALID;
static std::unordered_map<INSTANCE_HANDLE, winrt::IModule> s_instances;
HRESULT GetModule(INSTANCE_HANDLE id, _Out_ winrt::IModule& module)
{
    if (id < INSTANCE_HANDLE_START)
    {
        IFR(E_INVALIDARG);
    }

    auto it = s_instances.find(id);
    if (it == s_instances.end())
    {
        IFR(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
    }

    module = it->second;

    NULL_CHK_HR(module, E_POINTER);

    return S_OK;
}

static std::shared_ptr<IUnityDeviceResource> s_deviceResource;
static UnityGfxRenderer s_deviceType = kUnityGfxRendererNull;
static IUnityInterfaces* s_unityInterfaces = nullptr;
static IUnityGraphics* s_unityGraphics = nullptr;

HRESULT TrackModule(winrt::IModule &module, INSTANCE_HANDLE * handleId)
{
    // get the last index value;
    auto handle = s_lastPluginHandleIndex;

    // add to map
    auto success = s_instances.emplace(handle, module);
    if (success.second)
    {
        *handleId = handle++;
        s_lastPluginHandleIndex = handle;
    }

    return (success.second ? S_OK : E_UNEXPECTED);
}

// --------------------------------------------------------------------------
// UnitySetInterfaces
static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    s_lastPluginHandleIndex = INSTANCE_HANDLE_START;
    s_instances.clear();

    s_unityInterfaces = unityInterfaces;
    s_unityGraphics = s_unityInterfaces->Get<IUnityGraphics>();
    s_unityGraphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

    // Run OnGraphicsDeviceEvent(initialize) manually on plugin load
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    for (auto&& kv : s_instances)
    {
        kv.second.Shutdown();
        kv.second = nullptr;
    }
    s_instances.clear();

    s_lastPluginHandleIndex = INSTANCE_HANDLE_INVALID;

    s_unityGraphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

// --------------------------------------------------------------------------
// GraphicsDeviceEvent
static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    // Create graphics API implementation upon initialization
    if (eventType == kUnityGfxDeviceEventInitialize)
    {
        assert(s_deviceResource == nullptr);

        s_deviceType = s_unityGraphics->GetRenderer();

        s_deviceResource = CreateDeviceResource(s_deviceType);
    }

    // Let the implementation process the device related event
    if (s_deviceResource != nullptr)
    {
        s_deviceResource->ProcessDeviceEvent(eventType, s_unityInterfaces);
    }

    // Cleanup graphics API implementation upon shutdown
    if (eventType == kUnityGfxDeviceEventShutdown)
    {
        s_deviceResource.reset();
        s_deviceResource = nullptr;
        s_deviceType = kUnityGfxRendererNull;
    }
}


// --------------------------------------------------------------------------
// OnRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent. In this example, we just ignore
// that value.
static void UNITY_INTERFACE_API OnRenderEvent(int32_t eventID)
{
    DWORD dwId = static_cast<DWORD>(eventID);

    INSTANCE_HANDLE id = static_cast<INSTANCE_HANDLE>(LOWORD(dwId));

    winrt::IModule module = nullptr;
    if (SUCCEEDED(GetModule(id, module)))
    {
        uint16_t frameId = static_cast<uint16_t>(HIWORD(dwId));

        module.OnRenderEvent(frameId);
    }
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
    return OnRenderEvent;
}


// --------------------------------------------------------------------------
// Other function
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ReleaseInstance(
    _In_ INSTANCE_HANDLE id)
{
    winrt::IModule module = nullptr;
    if (SUCCEEDED(GetModule(id, module)))
    {
        s_instances.erase(id);
        module.Shutdown();
        module = nullptr;
    }
}


// Media Player
extern "C" int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API MediaPlayerCreatePlayer(
    _In_ StateChangedCallback fnCallback,
    _In_ void* managedObject,
    _Out_ INSTANCE_HANDLE* handleId)
{
    if (managedObject == nullptr)
    {
        return E_INVALIDARG;
    }
    winrt::IModule module = impl::PlaybackManager::Create(s_deviceResource, fnCallback, managedObject);

    return TrackModule(module, handleId);
}

extern "C" int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API MediaPlayerCreateTexture(
    _In_ INSTANCE_HANDLE id,
    _In_ int32_t width,
    _In_ int32_t height,
    _In_ void** playbackTexture)
{
    winrt::IModule module = nullptr;
    HRESULT hr = GetModule(id, module);
    if (SUCCEEDED(hr))
    {
        auto mediaPlayer = module.as<IPlaybackManagerPriv>();

        NULL_CHK_HR(mediaPlayer, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));

        hr = mediaPlayer->CreatePlaybackTexture(static_cast<uint32_t>(width), static_cast<uint32_t>(height), playbackTexture);
    }

    return hr;
}

extern "C" int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API MediaPlayerLoadContent(
    _In_ INSTANCE_HANDLE id,
    _In_ LPCWSTR contentLocation)
{
    winrt::IModule module = nullptr;
    HRESULT hr = GetModule(id, module);
    if (SUCCEEDED(hr))
    {
        auto mediaPlayer = module.as<IPlaybackManagerPriv>();

        NULL_CHK_HR(mediaPlayer, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));

        hr = mediaPlayer->LoadContent(contentLocation);
    }

    return hr;
}

extern "C" int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API MediaPlayerPlay(
    _In_ INSTANCE_HANDLE id)
{
    winrt::IModule module = nullptr;
    HRESULT hr = GetModule(id, module);
    if (SUCCEEDED(hr))
    {
        auto mediaPlayer = module.as<IPlaybackManagerPriv>();

        NULL_CHK_HR(mediaPlayer, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));

        hr = mediaPlayer->Play();
    }

    return hr;
}

extern "C" int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API MediaPlayerPause(
    _In_ INSTANCE_HANDLE id)
{
    winrt::IModule module = nullptr;
    HRESULT hr = GetModule(id, module);
    if (SUCCEEDED(hr))
    {
        auto mediaPlayer = module.as<IPlaybackManagerPriv>();

        NULL_CHK_HR(mediaPlayer, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));

        hr = mediaPlayer->Pause();
    }

    return hr;
}

extern "C" int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API MediaPlayerStop(
    _In_ INSTANCE_HANDLE id)
{
    winrt::IModule module = nullptr;
    HRESULT hr = GetModule(id, module);
    if (SUCCEEDED(hr))
    {
        auto mediaPlayer = module.as<IPlaybackManagerPriv>();

        NULL_CHK_HR(mediaPlayer, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));

        hr = mediaPlayer->Stop();
    }

    return hr;
}
