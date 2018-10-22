#pragma once

#include "Plugin/CaptureEngine.g.h"
#include "Plugin.Module.h"
#include "Media.Capture.Sink.h"
#include "Media.Capture.PayloadHandler.h"
#include "Media.SharedTexture.h"
#include "completion_source.h"

#include <mfapi.h>
#include <winrt/windows.media.h>
#include <winrt/Windows.Media.Capture.h>

namespace winrt::CameraCapture::Plugin::implementation
{
    struct CaptureEngine : CaptureEngineT<CaptureEngine, Module>
    {
        static Plugin::IModule Create(
            _In_ std::weak_ptr<IUnityDeviceResource> const& unityDevice,
            _In_ StateChangedCallback fnCallback);

        CaptureEngine();

        virtual void Shutdown();

        HRESULT StartPreview(uint32_t width, uint32_t height, bool enableAudio, bool enableMrc);
        HRESULT StopPreview();

        Windows::Perception::Spatial::SpatialCoordinateSystem AppCoordinateSystem()
        {
            return m_appCoordinateSystem;
        }
        void AppCoordinateSystem(Windows::Perception::Spatial::SpatialCoordinateSystem const& value)
        {
            m_appCoordinateSystem = value;
        }

    private:
        Windows::Foundation::IAsyncAction StartPreviewAsync(uint32_t width, uint32_t height, boolean enableAudio, boolean enableMrc);
        Windows::Foundation::IAsyncAction StopPreviewAsync();

        Windows::Foundation::IAsyncAction CreateMediaCaptureAsync(boolean enableAudio, Windows::Media::Capture::MediaCategory category);
        Windows::Foundation::IAsyncAction ReleaseMediaCaptureAsync();

        HRESULT CreateDeviceResources();
        void ReleaseDeviceResources();

        Windows::Foundation::IAsyncAction AddMrcEffectsAsync(boolean enableAudio);
        Windows::Foundation::IAsyncAction RemoveMrcEffectsAsync();

    private:
        slim_mutex m_mutex;
        std::atomic<boolean> m_isShutdown;

        Windows::Foundation::IAsyncAction m_startPreviewOp;
        Windows::Foundation::IAsyncAction m_stopPreviewOp;
        completion_source<boolean> m_stopCompletion;

        com_ptr<ID3D11Device> m_d3dDevice;
        uint32_t m_resetToken;
        com_ptr<IMFDXGIDeviceManager> m_dxgiDeviceManager;

        // media capture
        Windows::Media::Capture::MediaStreamType m_streamType;
        Windows::Media::Capture::MediaCapture m_mediaCapture;
        event_token m_failedEventToken;

        Windows::Media::IMediaExtension m_mrcAudioEffect;
        Windows::Media::IMediaExtension m_mrcVideoEffect;
        Windows::Media::IMediaExtension m_mrcPreviewEffect;

        // IMFMediaSink
        Media::Capture::Sink m_mediaSink;
        Media::Capture::PayloadHandler m_payloadHandler;
        event_token m_sampleEventToken;

        // buffers
        com_ptr<IMFSample> m_audioSample;
        com_ptr<SharedTexture> m_videoBuffer;

        Windows::Perception::Spatial::SpatialCoordinateSystem m_appCoordinateSystem;
    };
}

namespace winrt::CameraCapture::Plugin::factory_implementation
{
    struct CaptureEngine : CaptureEngineT<CaptureEngine, implementation::CaptureEngine>
    {
    };
}
