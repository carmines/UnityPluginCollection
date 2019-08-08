#pragma once

#include "Plugin.PlaybackManager.g.h"
#include "Plugin.Module.h"
#include "D3D11DeviceResources.h"
#include "MediaHelpers.h"

#include <winrt/Windows.Media.Playback.h>

struct __declspec(uuid("905a0fef-bc53-11df-8c49-001e4fc686da")) IPlaybackManagerPriv : ::IUnknown
{
    STDMETHOD(CreatePlaybackTexture)(_In_ uint32_t width, _In_ uint32_t height, _COM_Outptr_ void** ppvTexture) PURE;
    STDMETHOD(LoadContent)(_In_ winrt::hstring const& contentLocatio) PURE;
    STDMETHOD(Play)() PURE;
    STDMETHOD(Pause)() PURE;
    STDMETHOD(Stop)() PURE;
};

namespace winrt::VideoPlayer::Plugin::implementation
{
    struct PlaybackManager : PlaybackManagerT<PlaybackManager, Module, IPlaybackManagerPriv>
    {

        static Plugin::IModule Create(
            _In_ std::weak_ptr<IUnityDeviceResource> const& unityDevice,
            _In_ StateChangedCallback fnCallback,
            _In_ void* pCallbackObject);

        PlaybackManager();

        virtual void Shutdown();

        event_token Closed(Windows::Foundation::EventHandler<Plugin::PlaybackManager> const& handler);
        void Closed(event_token const& token);

        // IPlaybackManagerPriv
        STDOVERRIDEMETHODIMP CreatePlaybackTexture(_In_ UINT32 width, _In_ UINT32 height, _COM_Outptr_ void** ppvTexture);
        STDOVERRIDEMETHODIMP LoadContent(_In_ hstring const& pszContentLocation);
        STDOVERRIDEMETHODIMP Play();
        STDOVERRIDEMETHODIMP Pause();
        STDOVERRIDEMETHODIMP Stop();

    private:
        HRESULT CreateMediaPlayer();
        void ReleaseMediaPlayer();

        HRESULT CreateResources(com_ptr<ID3D11Device> const& unityDevice);
        void ReleaseResources();

    private:
        com_ptr<ID3D11Device> m_d3dDevice;
        uint32_t m_resetToken;
        com_ptr<IMFDXGIDeviceManager> m_dxgiDeviceManager;

        Windows::Media::Playback::MediaPlayer m_mediaPlayer;
        event_token m_endedToken;
        event_token m_failedToken;
        event_token m_openedToken;
        event_token m_videoFrameAvailableToken;

        Windows::Media::Playback::MediaPlaybackSession m_mediaPlaybackSession;
        event_token m_stateChangedEventToken;

        std::shared_ptr<SharedTextureBuffer> m_primaryBuffer;

        event<Windows::Foundation::EventHandler<Plugin::PlaybackManager>> m_closedEvent;
    };
}

namespace winrt::VideoPlayer::Plugin::factory_implementation
{
    struct PlaybackManager : PlaybackManagerT<PlaybackManager, implementation::PlaybackManager>
    {
    };
}
