// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Plugin.CaptureEngine.g.h"
#include "Plugin.Module.h"
#include "Media.PayloadHandler.h"
#include "Media.SharedTexture.h"
#include "Media.Capture.Sink.h"
#include "Media.Transform.h"
#include "Media.Payload.h"

#include <mfapi.h>
#include <winrt/windows.media.h>
#include <winrt/Windows.Media.Capture.h>

namespace winrt::CameraCapture::Plugin::implementation
{
    struct CaptureEngine : CaptureEngineT<CaptureEngine, Module>
    {
        static Plugin::Module Create(
            _In_ std::weak_ptr<IUnityDeviceResource> const& unityDevice,
            _In_ StateChangedCallback fnCallback,
            _In_ void* pCallbackObject);

        CaptureEngine();
        ~CaptureEngine() { Shutdown(); }

        virtual void Shutdown() override;

        hresult StartPreview(uint32_t width, uint32_t height, bool enableAudio, bool enableMrc);
        hresult StopPreview();
        hresult TakePhoto(uint32_t width, uint32_t height, bool enableMrc, Windows::Perception::Spatial::SpatialCoordinateSystem const& coordinateSystem);

        CameraCapture::Media::Capture::Sink MediaSink();

        CameraCapture::Media::PayloadHandler PayloadHandler();
        void PayloadHandler(CameraCapture::Media::PayloadHandler const& value);


    private:
        hresult CreateDeviceResources();
        void ReleaseDeviceResources();

        Windows::Foundation::IAsyncAction StartPreviewCoroutine(uint32_t const width, uint32_t const height, boolean const enableAudio, boolean const enableMrc);
        Windows::Foundation::IAsyncAction StopPreviewCoroutine();
        Windows::Foundation::IAsyncOperation<CameraCapture::Media::Payload> TakePhotoCoroutine(
            uint32_t width, uint32_t height, boolean enableMrc, 
            Windows::Perception::Spatial::SpatialCoordinateSystem coordinateSystem);

        Windows::Foundation::IAsyncAction CreateMediaCaptureAsync(uint32_t const& width, uint32_t const& height, boolean const& enableAudio);
        Windows::Foundation::IAsyncAction ReleaseMediaCaptureAsync();

        Windows::Foundation::IAsyncAction AddMrcEffectsAsync(boolean const enableAudio);
        Windows::Foundation::IAsyncAction RemoveMrcEffectsAsync();

        hresult CreatePhotoTexture(uint32_t width, uint32_t height);

    private:
        CriticalSection m_cs;

        std::atomic<boolean> m_isShutdown;
        winrt::handle m_startPreviewEventHandle;
        winrt::handle m_stopPreviewEventHandle;
        winrt::handle m_takePhotoEventHandle;

        com_ptr<ID3D11Device> m_mediaDevice;
        uint32_t m_resetToken;
        com_ptr<IMFDXGIDeviceManager> m_dxgiDeviceManager;

        Windows::Foundation::IAsyncAction m_startPreviewOp;
        Windows::Foundation::IAsyncAction m_stopPreviewOp;
        Windows::Foundation::IAsyncOperation<Media::Payload> m_takePhotoOp;

        // media capture
        Windows::Media::Capture::MediaCategory m_category;
        Windows::Media::Capture::MediaStreamType m_streamType;
        Windows::Media::Capture::KnownVideoProfile m_videoProfile;
        Windows::Media::Capture::MediaCaptureSharingMode m_sharingMode;
        Windows::Media::Capture::MediaCapture m_mediaCapture;
        Windows::Media::Capture::MediaCaptureInitializationSettings m_initSettings;

        Windows::Media::IMediaExtension m_mrcAudioEffect;
        Windows::Media::IMediaExtension m_mrcVideoEffect;
        Windows::Media::IMediaExtension m_mrcPreviewEffect;

        // IMFMediaSink
        Media::Capture::Sink m_mediaSink;

        Media::PayloadHandler m_payloadHandler;
        Media::PayloadHandler::OnStreamPayload_revoker m_payloadEventRevoker;

        // buffers
        com_ptr<IMFSample> m_audioSample;
        com_ptr<SharedTexture> m_sharedVideoTexture;

        CD3D11_TEXTURE2D_DESC m_photoTextureDesc;
        com_ptr<ID3D11Texture2D> m_photoTexture;
        com_ptr<ID3D11ShaderResourceView> m_photoTextureSRV;
        com_ptr<IMFSample> m_photoSample;
        com_ptr<ITransformPriv> m_transform;
    };
}

namespace winrt::CameraCapture::Plugin::factory_implementation
{
    struct CaptureEngine : CaptureEngineT<CaptureEngine, implementation::CaptureEngine>
    {
    };
}
