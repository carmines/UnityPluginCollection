// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Plugin.CaptureEngine.h"
#include "Plugin.CaptureEngine.g.cpp"

#include "Media.Functions.h"
#include "Media.Capture.MrcAudioEffect.h"
#include "Media.Capture.MrcVideoEffect.h"

#include <mferror.h>
#include <mfmediacapture.h>

#include <winrt/windows.media.devices.h>
#include <pplawait.h>

using namespace winrt;
using namespace CameraCapture::Plugin::implementation;
using namespace CameraCapture::Media::Capture::implementation;
using namespace Windows::Foundation;
using namespace Windows::Media::Effects;
using namespace Windows::Media::Core;
using namespace Windows::Media::Capture;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Perception::Spatial;

_Use_decl_annotations_
CameraCapture::Plugin::Module CaptureEngine::Create(
    std::weak_ptr<IUnityDeviceResource> const& unityDevice,
    StateChangedCallback fnCallback,
    void* pCallbackObject)
{
    auto capture = CameraCapture::Plugin::CaptureEngine();
    if (SUCCEEDED(capture.as<IModulePriv>()->Initialize(unityDevice, fnCallback, pCallbackObject)))
    {
        return capture;
    }

    return nullptr;
}


CaptureEngine::CaptureEngine()
    : m_isShutdown(false)
    , m_startPreviewEventHandle(CreateEvent(nullptr, true, true, nullptr))
    , m_stopPreviewEventHandle(CreateEvent(nullptr, true, true, nullptr))
    , m_takePhotoEventHandle(CreateEvent(nullptr, true, true, nullptr))
    , m_mediaDevice(nullptr)
    , m_dxgiDeviceManager(nullptr)
    , m_category(MediaCategory::Communications)
    , m_streamType(MediaStreamType::VideoPreview)
    , m_videoProfile(KnownVideoProfile::VideoConferencing)
    , m_sharingMode(MediaCaptureSharingMode::ExclusiveControl)
    , m_startPreviewOp(nullptr)
    , m_stopPreviewOp(nullptr)
    , m_mediaCapture(nullptr)
    , m_initSettings(nullptr)
    , m_mrcAudioEffect(nullptr)
    , m_mrcVideoEffect(nullptr)
    , m_mrcPreviewEffect(nullptr)
    , m_mediaSink(nullptr)
    , m_payloadHandler(nullptr)
    , m_audioSample(nullptr)
    , m_sharedVideoTexture(nullptr)
    , m_photoTexture(nullptr)
    , m_photoTextureSRV(nullptr)
    , m_photoSample(nullptr)
    , m_transform(nullptr)
{
}

void CaptureEngine::Shutdown()
{
    auto strong = get_strong();

    if (m_isShutdown)
    {
        return;
    }
    m_isShutdown = true;

    // see if any outstanding operations are running
    if (m_startPreviewOp != nullptr && m_startPreviewOp.Status() == AsyncStatus::Started)
    {
        concurrency::create_task([this, strong]()
            {
                WaitForSingleObject(m_startPreviewEventHandle.get(), 15000);
            }).get();
    }

    if (m_takePhotoOp != nullptr && m_takePhotoOp.Status() == AsyncStatus::Started)
    {
        concurrency::create_task([this, strong]()
            {
                WaitForSingleObject(m_takePhotoEventHandle.get(), 15000);
            }).get();
    }

    if (m_mediaCapture != nullptr)
    {
        StopPreview();
    }

    if (m_stopPreviewOp != nullptr && m_stopPreviewOp.Status() == AsyncStatus::Started)
    {
        concurrency::create_task([this, strong]()
            {
                WaitForSingleObject(m_stopPreviewEventHandle.get(), 15000);
            }).get();
    }

    if (m_mediaSink != nullptr)
    {
        auto mfSink = m_mediaSink.try_as<IMFMediaSink>();
        if (mfSink != nullptr)
        {
            mfSink->Shutdown();
        }

        m_mediaSink = nullptr;
    }

    ReleaseDeviceResources();

    Module::Shutdown();
}

hresult CaptureEngine::StartPreview(uint32_t width, uint32_t height, bool enableAudio, bool enableMrc)
{
    if (m_startPreviewOp != nullptr)
    {
        IFR(E_ABORT);
    }

    if (m_stopPreviewOp != nullptr && m_stopPreviewOp.Status() == AsyncStatus::Started)
    {
        concurrency::create_task([this]()
            {
                WaitForSingleObject(m_stopPreviewEventHandle.get(), INFINITE);
            }).get();
    }

    if (m_takePhotoOp != nullptr && m_takePhotoOp.Status() == AsyncStatus::Started)
    {
        concurrency::create_task([this]()
            {
                WaitForSingleObject(m_takePhotoEventHandle.get(), INFINITE);
            }).get();
    }

    ResetEvent(m_startPreviewEventHandle.get());

    IFR(CreateDeviceResources());

    if (m_sharedVideoTexture != nullptr)
    {
        m_sharedVideoTexture->Reset();

        m_sharedVideoTexture = nullptr;
    }

    m_startPreviewOp = StartPreviewCoroutine(width, height, enableAudio, enableMrc);
    m_startPreviewOp.Completed([this, strong = get_strong()](auto const& result, auto const& status)
    {
        m_startPreviewOp = nullptr;

        if (status == AsyncStatus::Error)
        {
            Failed(result.ErrorCode());
        }
        else if (status == AsyncStatus::Completed)
        {
            CALLBACK_STATE state{};
            ZeroMemory(&state, sizeof(CALLBACK_STATE));

            state.type = CallbackType::Capture;

            ZeroMemory(&state.value.captureState, sizeof(CAPTURE_STATE));

            state.value.captureState.stateType = CaptureStateType::PreviewStarted;

            Callback(state);
        }
    });

    return S_OK;
}

hresult CaptureEngine::StopPreview()
{
    if (m_stopPreviewOp != nullptr)
    {
        IFR(E_ABORT);
    }

    if (m_startPreviewOp != nullptr && m_startPreviewOp.Status() == AsyncStatus::Started)
    {
        concurrency::create_task([this]()
            {
                WaitForSingleObject(m_startPreviewEventHandle.get(), INFINITE);
            }).get();
    }

    if (m_takePhotoOp != nullptr && m_takePhotoOp.Status() == AsyncStatus::Started)
    {
        concurrency::create_task([this]()
            {
                WaitForSingleObject(m_takePhotoEventHandle.get(), INFINITE);
            }).get();
    }

    ResetEvent(m_stopPreviewEventHandle.get());

    m_stopPreviewOp = StopPreviewCoroutine();
    m_stopPreviewOp.Completed([this, strong = get_strong()](auto const& result, auto const& status)
    {
        m_stopPreviewOp = nullptr;

        if (status == AsyncStatus::Error)
        {
            Failed(result.ErrorCode());
        }
        else if (status == AsyncStatus::Completed)
        {
            CALLBACK_STATE state{};
            ZeroMemory(&state, sizeof(CALLBACK_STATE));

            state.type = CallbackType::Capture;

            ZeroMemory(&state.value.captureState, sizeof(CAPTURE_STATE));

            state.value.captureState.stateType = CaptureStateType::PreviewStopped;

            Callback(state);
        }
    });

    return S_OK;
}

hresult CaptureEngine::TakePhoto(uint32_t width, uint32_t height, bool enableMrc, SpatialCoordinateSystem const& coordinateSystem)
{
    if (m_takePhotoOp)
    {
        IFR(E_ABORT);
    }

    if (m_stopPreviewOp != nullptr && m_stopPreviewOp.Status() == AsyncStatus::Started)
    {
        concurrency::create_task([this]()
            {
                WaitForSingleObject(m_stopPreviewEventHandle.get(), INFINITE);
            }).get();
    }

    if (m_stopPreviewOp != nullptr && m_stopPreviewOp.Status() == AsyncStatus::Started)
    {
        concurrency::create_task([this]()
            {
                WaitForSingleObject(m_stopPreviewEventHandle.get(), INFINITE);
            }).get();
    }

    ResetEvent(m_takePhotoEventHandle.get());

    IFR(CreateDeviceResources());

    m_takePhotoOp = TakePhotoCoroutine(width, height, enableMrc, coordinateSystem);
    m_takePhotoOp.Completed([this, strong = get_strong()](auto const& result, auto const& status)
    {
        auto payload = m_takePhotoOp.GetResults();

        m_takePhotoOp = nullptr;

        if (status == AsyncStatus::Error)
        {
            Failed(result.ErrorCode());
        }
        else if (status == AsyncStatus::Completed)
        {
            CALLBACK_STATE state{};
            ZeroMemory(&state, sizeof(CALLBACK_STATE));

            state.type = CallbackType::Capture;

            ZeroMemory(&state.value.captureState, sizeof(CAPTURE_STATE));

            state.value.captureState.stateType = CaptureStateType::PhotoFrame;

            // Set state values
            state.value.captureState.width = m_photoTextureDesc.Width;
            state.value.captureState.height = m_photoTextureDesc.Height;
            state.value.captureState.texturePtr = m_photoTextureSRV.get();
            if (payload != nullptr)
            {
                state.value.captureState.worldMatrix = payload.CameraToWorld();
                state.value.captureState.projectionMatrix = payload.CameraProjection();
            }

            Callback(state);
        }
    });

    return S_OK;
}


CameraCapture::Media::PayloadHandler CaptureEngine::PayloadHandler()
{
    auto guard = m_cs.Guard();

    return m_payloadHandler;
}
void CaptureEngine::PayloadHandler(CameraCapture::Media::PayloadHandler const& value)
{
    auto strong = get_strong();

    m_payloadHandler = value;

    if (m_mediaSink != nullptr)
    {
        m_mediaSink.PayloadHandler(m_payloadHandler);
    }

    m_payloadEventRevoker = m_payloadHandler.OnStreamPayload(winrt::auto_revoke, [this, strong](auto const sender, Media::Payload const& payload)
        {
            auto guard = m_cs.Guard();

            if (m_isShutdown)
            {
                return;
            }

            if (sender != m_payloadHandler)
            {
                return;
            }

            if (payload == nullptr)
            {
                return;
            }

            GUID majorType = GUID_NULL;

            auto mediaStreamSample = payload.MediaStreamSample();
            if (mediaStreamSample != nullptr)
            {
                auto type = mediaStreamSample.ExtendedProperties().TryLookup(MF_MT_MAJOR_TYPE);
                if (type != nullptr)
                {
                    majorType = winrt::unbox_value<guid>(type);
                }
            }

            auto streamSample = payload.as<IStreamSample>();
            if (streamSample == nullptr)
            {
                return;
            }

            if (MFMediaType_Audio == majorType)
            {
                if (m_audioSample == nullptr)
                {
                    DWORD bufferSize = 0;
                    IFV(streamSample->Sample()->GetTotalLength(&bufferSize));

                    com_ptr<IMFMediaBuffer> dstBuffer = nullptr;
                    IFV(MFCreateMemoryBuffer(bufferSize, dstBuffer.put()));

                    com_ptr<IMFSample> dstSample = nullptr;
                    IFV(MFCreateSample(dstSample.put()));

                    IFV(dstSample->AddBuffer(dstBuffer.get()));

                    m_audioSample.attach(dstSample.detach());
                }

                IFV(CopySample(MFMediaType_Audio, streamSample->Sample(), m_audioSample));

                CALLBACK_STATE state{};
                ZeroMemory(&state, sizeof(CALLBACK_STATE));

                state.type = CallbackType::Capture;

                ZeroMemory(&state.value.captureState, sizeof(CAPTURE_STATE));

                state.value.captureState.stateType = CaptureStateType::PreviewAudioFrame;
                //state.value.captureState.width = 0;
                //state.value.captureState.height = 0;
                //state.value.captureState.texturePtr = nullptr;
                Callback(state);
            }
            else if (MFMediaType_Video == majorType)
            {
                boolean bufferChanged = false;

                auto videoProps = payload.EncodingProperties().as<IVideoEncodingProperties>();

                if (m_sharedVideoTexture == nullptr
                    ||
                    m_sharedVideoTexture->frameTexture == nullptr
                    ||
                    m_sharedVideoTexture->frameTextureDesc.Width != videoProps.Width()
                    ||
                    m_sharedVideoTexture->frameTextureDesc.Height != videoProps.Height())
                {
                    auto resources = m_d3d11DeviceResources.lock();
                    NULL_CHK_R(resources);

                    // make sure we have created our own d3d device
                    IFV(CreateDeviceResources());

                    IFV(SharedTexture::Create(resources->GetDevice(), m_dxgiDeviceManager, videoProps.Width(), videoProps.Height(), m_sharedVideoTexture));

                    bufferChanged = true;
                }

                // copy the data
                IFV(CopySample(MFMediaType_Video, streamSample->Sample(), m_sharedVideoTexture->mediaSample));

                // did the texture description change, if so, raise callback
                CALLBACK_STATE state{};
                ZeroMemory(&state, sizeof(CALLBACK_STATE));

                state.type = CallbackType::Capture;

                ZeroMemory(&state.value.captureState, sizeof(CAPTURE_STATE));

                state.value.captureState.stateType = CaptureStateType::PreviewVideoFrame;
                state.value.captureState.width = m_sharedVideoTexture->frameTextureDesc.Width;
                state.value.captureState.height = m_sharedVideoTexture->frameTextureDesc.Height;
                state.value.captureState.texturePtr = m_sharedVideoTexture->frameTextureSRV.get();
                if (m_payloadHandler.ProceesTranform(payload))
                {
                    state.value.captureState.worldMatrix = payload.CameraToWorld();
                    state.value.captureState.projectionMatrix = payload.CameraProjection();

                    bufferChanged = true;
                }

                if (bufferChanged)
                {
                    Callback(state);
                }
            }
        });
}

CameraCapture::Media::Capture::Sink CaptureEngine::MediaSink()
{
    auto guard = m_cs.Guard();

    return m_mediaSink;
}

// private
hresult CaptureEngine::CreateDeviceResources()
{
    if (m_mediaDevice != nullptr && m_dxgiDeviceManager != nullptr)
    {
        return S_OK;
    }

    // get the adapter from an existing d3dDevice
    auto resources = m_d3d11DeviceResources.lock();
    NULL_CHK_HR(resources, MF_E_UNEXPECTED);

    com_ptr<IDXGIAdapter> dxgiAdapter = nullptr;
    com_ptr<IDXGIDevice> dxgiDevice = resources->GetDevice().try_as<IDXGIDevice>();
    if (dxgiDevice != nullptr)
    {
        IFR(dxgiDevice->GetAdapter(dxgiAdapter.put()));
    }

    com_ptr<ID3D11Device> mediaDevice = nullptr;
    IFR(CreateMediaDevice(dxgiAdapter.get(), mediaDevice.put()));

    // create DXGIManager
    uint32_t resetToken;
    com_ptr<IMFDXGIDeviceManager> dxgiDeviceManager = nullptr;
    IFR(MFCreateDXGIDeviceManager(&resetToken, dxgiDeviceManager.put()));

    // associate device with dxgiManager
    IFR(dxgiDeviceManager->ResetDevice(mediaDevice.get(), resetToken));

    // success, store the values
    m_mediaDevice.attach(mediaDevice.detach());
    m_dxgiDeviceManager.attach(dxgiDeviceManager.detach());
    m_resetToken = resetToken;

    return S_OK;
}

void CaptureEngine::ReleaseDeviceResources()
{
    if (m_audioSample != nullptr)
    {
        m_audioSample = nullptr;
    }

    if (m_sharedVideoTexture != nullptr)
    {
        m_sharedVideoTexture->Reset();

        m_sharedVideoTexture = nullptr;
    }

    if (m_photoTexture != nullptr)
    {
        m_photoTexture = nullptr;
    }

    if (m_photoTextureSRV != nullptr)
    {
        m_photoTextureSRV = nullptr;
    }

    if (m_dxgiDeviceManager != nullptr)
    {
        if (m_mediaDevice != nullptr)
        {
            m_dxgiDeviceManager->ResetDevice(nullptr, m_resetToken);

            m_mediaDevice = nullptr;
        }

        m_dxgiDeviceManager = nullptr;
    }

    if (m_mediaDevice != nullptr)
    {
        m_mediaDevice = nullptr;
    }
}


IAsyncAction CaptureEngine::StartPreviewCoroutine(
    uint32_t width, uint32_t height,
    boolean enableAudio, boolean enableMrc)
{
    winrt::apartment_context calling_thread;

    co_await resume_background();

    auto guard = m_cs.Guard();

    if (m_mediaCapture == nullptr)
    {
        co_await CreateMediaCaptureAsync(width, height, enableAudio);
    }
    else
    {
        co_await RemoveMrcEffectsAsync();
    }

    // set video controller properties
    auto videoController = m_mediaCapture.VideoDeviceController();

    try
    {
        videoController.DesiredOptimization(Windows::Media::Devices::MediaCaptureOptimization::LatencyThenQuality);
    }
    catch (hresult_error const& er)
    {
        Log(L"DesiredOptimization failed: 0x%lx", er.code());
    }

    // override video controller media stream properties
    if (m_initSettings.SharingMode() == MediaCaptureSharingMode::ExclusiveControl)
    {
        auto videoEncProps = GetVideoDeviceProperties(videoController, m_streamType, width, height, MediaEncodingSubtypes::Nv12());
        co_await videoController.SetMediaStreamPropertiesAsync(m_streamType, videoEncProps);

        auto captureSettings = m_mediaCapture.MediaCaptureSettings();
        if (m_streamType != MediaStreamType::VideoPreview
            &&
            captureSettings.VideoDeviceCharacteristic() != VideoDeviceCharacteristic::AllStreamsIdentical
            &&
            captureSettings.VideoDeviceCharacteristic() != VideoDeviceCharacteristic::PreviewRecordStreamsIdentical)
        {
            videoEncProps = GetVideoDeviceProperties(videoController, MediaStreamType::VideoRecord, width, height, MediaEncodingSubtypes::Nv12());
            co_await videoController.SetMediaStreamPropertiesAsync(MediaStreamType::VideoRecord, videoEncProps);
        }
    }

    // encoding profile based on 720p
    auto encodingProfile = MediaEncodingProfile::CreateMp4(VideoEncodingQuality::HD720p);
    encodingProfile.Container(nullptr);

    // update the audio profile to match device
    if (enableAudio)
    {
        auto audioController = m_mediaCapture.AudioDeviceController();
        auto audioMediaProperties = audioController.GetMediaStreamProperties(MediaStreamType::Audio);
        auto audioMediaProperty = audioMediaProperties.as<AudioEncodingProperties>();

        encodingProfile.Audio().Bitrate(audioMediaProperty.Bitrate());
        encodingProfile.Audio().BitsPerSample(audioMediaProperty.BitsPerSample());
        encodingProfile.Audio().ChannelCount(audioMediaProperty.ChannelCount());
        encodingProfile.Audio().SampleRate(audioMediaProperty.SampleRate());
        if (m_streamType == MediaStreamType::VideoPreview) // for local playback only
        {
            encodingProfile.Audio().Subtype(MediaEncodingSubtypes::Float());
        }
    }
    else
    {
        encodingProfile.Audio(nullptr);
    }

    auto videoMediaProperty = videoController.GetMediaStreamProperties(m_streamType).as<VideoEncodingProperties>();
    if (videoMediaProperty != nullptr)
    {
        encodingProfile.Video().Width(videoMediaProperty.Width());
        encodingProfile.Video().Height(videoMediaProperty.Height());
        if (m_streamType == MediaStreamType::VideoPreview) // for local playback only
        {
            encodingProfile.Video().Subtype(MediaEncodingSubtypes::Bgra8());
        }
    }

    // media sink
    auto mediaSink = CameraCapture::Media::Capture::Sink(encodingProfile);

    // create mrc effects first
    if (enableMrc)
    {
        co_await AddMrcEffectsAsync(enableAudio);
    }

    if (m_streamType == MediaStreamType::VideoRecord)
    {
        co_await m_mediaCapture.StartRecordToCustomSinkAsync(encodingProfile, mediaSink);
    }
    else if (m_streamType == MediaStreamType::VideoPreview)
    {
        co_await m_mediaCapture.StartPreviewToCustomSinkAsync(encodingProfile, mediaSink);
    }

    // store locals
    m_mediaSink = mediaSink;

    if (m_payloadHandler != nullptr)
    {
        m_mediaSink.PayloadHandler(m_payloadHandler);
    }

    SetEvent(m_startPreviewEventHandle.get());

    co_await calling_thread;
}

IAsyncAction CaptureEngine::StopPreviewCoroutine()
{
    winrt::apartment_context calling_thread;

    co_await resume_background();

    auto guard = m_cs.Guard();

    m_payloadEventRevoker.revoke();

    m_payloadHandler = nullptr;

    if (m_mediaSink != nullptr)
    {
        m_mediaSink.PayloadHandler(nullptr);
    }

    hresult hr = S_OK;

    if (m_mediaCapture != nullptr)
    {
        try
        {
            if (m_mediaCapture.CameraStreamState() == Windows::Media::Devices::CameraStreamState::Streaming)
            {
                if (m_streamType == MediaStreamType::VideoRecord)
                {
                    co_await m_mediaCapture.StopRecordAsync();
                }
                else if (m_streamType == MediaStreamType::VideoPreview)
                {
                    co_await m_mediaCapture.StopPreviewAsync();
                }
            }
        }
        catch (hresult_error er)
        {
            Log(L"StopPreviewAsync failed: %s", er.message().c_str());

            hr = er.code();
        }

        co_await ReleaseMediaCaptureAsync();

        if (m_mediaSink != nullptr)
        {
            auto mfSink = m_mediaSink.try_as<IMFMediaSink>();
            if (mfSink != nullptr)
            {
                mfSink->Shutdown();
            }

            m_mediaSink = nullptr;
        }
    }

    SetEvent(m_stopPreviewEventHandle.get());

    if (FAILED(hr))
    {
        throw_hresult(hr);
    }

    co_await calling_thread;
}

IAsyncOperation<CameraCapture::Media::Payload> CaptureEngine::TakePhotoCoroutine(
    uint32_t width,
    uint32_t height,
    boolean enableMrc,
    SpatialCoordinateSystem coordinateSystem)
{
    winrt::apartment_context calling_thread;

    Media::Payload photoPayload = nullptr;

    co_await resume_background();

    auto guard = m_cs.Guard();

    auto createdCapture = false;
    if (m_mediaCapture == nullptr)
    {
        co_await CreateMediaCaptureAsync(width, height, false);

        createdCapture = true;
    }

    auto captureSettings = m_mediaCapture.MediaCaptureSettings();

    auto characteristic = captureSettings.VideoDeviceCharacteristic();

    auto addedEffect = false;
    if (enableMrc
        &&
        characteristic != VideoDeviceCharacteristic::AllStreamsIndependent
        &&
        characteristic != VideoDeviceCharacteristic::PreviewPhotoStreamsIdentical)
    {
        try
        {
            auto mrcVideoEffect = Media::Capture::MrcVideoEffect();
            mrcVideoEffect.StreamType(MediaStreamType::Photo);

            co_await m_mediaCapture.AddVideoEffectAsync(mrcVideoEffect, MediaStreamType::Photo);
        }
        catch (hresult_error const& error)
        {
            Log(L"can't add the mrc extension - %s\n", error.message().c_str());
        }

        addedEffect = true;
    }

    // set video controller properties
    auto videoController = m_mediaCapture.VideoDeviceController();

    // override video controller media stream properties
    if (m_initSettings.SharingMode() == MediaCaptureSharingMode::ExclusiveControl)
    {
        // find the closest resolution
        auto videoEncProps = GetVideoDeviceProperties(videoController, MediaStreamType::Photo, width, height, MediaEncodingSubtypes::Nv12());
        co_await videoController.SetMediaStreamPropertiesAsync(MediaStreamType::Photo, videoEncProps);
    }

    auto photoProps = videoController.GetMediaStreamProperties(MediaStreamType::Photo).as<VideoEncodingProperties>();

    if (m_photoSample == nullptr
        ||
        m_photoTextureDesc.Width != photoProps.Width()
        ||
        m_photoTextureDesc.Height != photoProps.Height())
    {
        CreatePhotoTexture(photoProps.Width(), photoProps.Height());
    }

    auto encProperties = ImageEncodingProperties::CreateUncompressed(MediaPixelFormat::Bgra8);
    encProperties.Width(photoProps.Width());
    encProperties.Height(photoProps.Height());

    auto photoCapture = co_await m_mediaCapture.PrepareLowLagPhotoCaptureAsync(encProperties);

    auto capturedPhoto = co_await photoCapture.CaptureAsync();

    com_ptr<IMFGetService> spService = capturedPhoto.Frame().try_as<IMFGetService>();
    if (spService != nullptr)
    {
        com_ptr<IMFMediaType> mediaType = nullptr;
        IFT(MFCreateMediaTypeFromProperties(winrt::get_unknown(encProperties), mediaType.put()));

        com_ptr<IMFSample> spSample = nullptr;
        IFT(spService->GetService(MF_WRAPPED_SAMPLE_SERVICE, __uuidof(IMFSample), spSample.put_void()));

        // get the transform
        if (coordinateSystem != nullptr)
        {
            Windows::Foundation::Numerics::float4x4 transform{};
            Windows::Foundation::Numerics::float4x4 projection{};
            if (SUCCEEDED(m_transform->GetTransforms(spSample, coordinateSystem, transform, projection)))
            {
                photoPayload = Media::Payload();
                IFT(photoPayload.as<IStreamSample>()->Sample(MFMediaType_Video, mediaType, spSample));
                photoPayload.as<IStreamSample>()->SetTransformAndProjection(transform, projection);
            }
        }

        // copy the data
        IFT(CopySample(MFMediaType_Video, spSample, m_photoSample));
    }

    co_await photoCapture.FinishAsync();

    if (addedEffect)
    {
        try
        {
            co_await m_mediaCapture.ClearEffectsAsync(MediaStreamType::Photo);
        }
        catch (hresult_error const& error)
        {
            Log(L"can't clear the mrc extension - %s", error.message().c_str());
        }
    }

    if (createdCapture)
    {
        co_await ReleaseMediaCaptureAsync();
    }

    SetEvent(m_takePhotoEventHandle.get());

    co_await calling_thread;

    co_return photoPayload;
}


IAsyncAction CaptureEngine::CreateMediaCaptureAsync(
    uint32_t const& width,
    uint32_t const& height,
    boolean const& enableAudio)
{
    if (m_mediaCapture != nullptr)
    {
        co_return;
    }

    auto audioDevice = co_await GetFirstDeviceAsync(Windows::Devices::Enumeration::DeviceClass::AudioCapture);
    auto videoDevice = co_await GetFirstDeviceAsync(Windows::Devices::Enumeration::DeviceClass::VideoCapture);

    // initialize settings
    auto initSettings = MediaCaptureInitializationSettings();
    initSettings.MemoryPreference(MediaCaptureMemoryPreference::Auto);
    initSettings.StreamingCaptureMode(enableAudio ? StreamingCaptureMode::AudioAndVideo : StreamingCaptureMode::Video);
    initSettings.MediaCategory(m_category);
    initSettings.VideoDeviceId(videoDevice.Id());
    if (enableAudio)
    {
        initSettings.AudioDeviceId(audioDevice.Id());
    }

    // which stream should photo capture use
    if (m_streamType == MediaStreamType::VideoPreview)
    {
        initSettings.PhotoCaptureSource(PhotoCaptureSource::VideoPreview);
    }
    else
    {
        initSettings.PhotoCaptureSource(PhotoCaptureSource::Auto);
    }

    // set the DXGIManger for the media capture
    auto advancedInitSettings = initSettings.as<IAdvancedMediaCaptureInitializationSettings>();
    IFT(advancedInitSettings->SetDirectxDeviceManager(m_dxgiDeviceManager.get()));

    // if profiles are supported
    if (MediaCapture::IsVideoProfileSupported(videoDevice.Id()))
    {
        initSettings.SharingMode(MediaCaptureSharingMode::SharedReadOnly);

        setlocale(LC_ALL, "");

        // set the profile / mediaDescription that matches
        MediaCaptureVideoProfile videoProfile = nullptr;
        MediaCaptureVideoProfileMediaDescription videoProfileMediaDescription = nullptr;
        auto profiles = MediaCapture::FindKnownVideoProfiles(videoDevice.Id(), m_videoProfile);
        for (auto const& profile : profiles)
        {
            auto const& videoProfileMediaDescriptions = m_streamType == (MediaStreamType::VideoPreview) ? profile.SupportedPreviewMediaDescription() : profile.SupportedRecordMediaDescription();
            auto const& found = std::find_if(begin(videoProfileMediaDescriptions), end(videoProfileMediaDescriptions), [&](MediaCaptureVideoProfileMediaDescription const& desc)
                {
                    Log(L"\tFormat: %s: %i x %i @ %f fps",
                        desc.Subtype().c_str(),
                        desc.Width(),
                        desc.Height(),
                        desc.FrameRate());

                    // store a default
                    if (videoProfile == nullptr)
                    {
                        videoProfile = profile;
                    }

                    if (videoProfileMediaDescription == nullptr)
                    {
                        videoProfileMediaDescription = desc;
                    }

                    // select a size that will be == width/height @ 30fps, final size will be set with enc props
                    bool match =
                        _wcsicmp(desc.Subtype().c_str(), MediaEncodingSubtypes::Nv12().c_str()) == 0 &&
                        desc.Width() == width &&
                        desc.Height() == height &&
                        desc.FrameRate() == 30.0;
                    if (match)
                    {
                        Log(L" - found\n");
                    }
                    else
                    {
                        Log(L"\n");
                    }

                    return match;
                });

            if (found != end(videoProfileMediaDescriptions))
            {
                videoProfile = profile;
                videoProfileMediaDescription = *found;
                break;
            }
        }

        initSettings.VideoProfile(videoProfile);
        if (m_streamType == MediaStreamType::VideoPreview)
        {
            initSettings.PreviewMediaDescription(videoProfileMediaDescription);
        }
        else
        {
            initSettings.RecordMediaDescription(videoProfileMediaDescription);
        }
        initSettings.PhotoMediaDescription(videoProfileMediaDescription);
    }
    else
    {
        initSettings.SharingMode(MediaCaptureSharingMode::ExclusiveControl);
    }

    auto mediaCapture = Windows::Media::Capture::MediaCapture();
    co_await mediaCapture.InitializeAsync(initSettings);

    m_mediaCapture = mediaCapture;
    m_initSettings = initSettings;
}

IAsyncAction CaptureEngine::ReleaseMediaCaptureAsync()
{
    if (m_mediaCapture == nullptr)
    {
        co_return;
    }

    co_await RemoveMrcEffectsAsync();

    if (m_initSettings != nullptr)
    {
        m_initSettings.as<IAdvancedMediaCaptureInitializationSettings>()->SetDirectxDeviceManager(nullptr);

        m_initSettings = nullptr;
    }

    m_mediaCapture.Close();

    m_mediaCapture = nullptr;
}


IAsyncAction CaptureEngine::AddMrcEffectsAsync(
    boolean enableAudio)
{
    if (m_mediaCapture == nullptr)
    {
        co_return;
    }

    auto captureSettings = m_mediaCapture.MediaCaptureSettings();

    try
    {
        auto mrcVideoEffect = Media::Capture::MrcVideoEffect();

        if (captureSettings.VideoDeviceCharacteristic() == VideoDeviceCharacteristic::AllStreamsIdentical ||
            captureSettings.VideoDeviceCharacteristic() == VideoDeviceCharacteristic::PreviewRecordStreamsIdentical)
        {
            mrcVideoEffect.StreamType(MediaStreamType::VideoRecord);
            m_mrcVideoEffect = co_await m_mediaCapture.AddVideoEffectAsync(mrcVideoEffect, MediaStreamType::VideoRecord);
        }
        else
        {
            mrcVideoEffect.StreamType(MediaStreamType::VideoPreview);
            m_mrcPreviewEffect = co_await m_mediaCapture.AddVideoEffectAsync(mrcVideoEffect, MediaStreamType::VideoPreview);

            mrcVideoEffect.StreamType(MediaStreamType::VideoRecord);
            m_mrcVideoEffect = co_await m_mediaCapture.AddVideoEffectAsync(mrcVideoEffect, MediaStreamType::VideoRecord);
        }

        if (enableAudio)
        {
            auto mrcAudioEffect = make<MrcAudioEffect>().as<IAudioEffectDefinition>();

            m_mrcAudioEffect = co_await m_mediaCapture.AddAudioEffectAsync(mrcAudioEffect);
        }
    }
    catch (hresult_error const& e)
    {
        Log(L"failed to add Mrc effects to streams: %s\n", e.message().c_str());
    }

    co_return;
}

IAsyncAction CaptureEngine::RemoveMrcEffectsAsync()
{
    if (m_mediaCapture == nullptr)
    {
        co_return;
    }

    if (m_mrcAudioEffect != nullptr || m_mrcPreviewEffect != nullptr || m_mrcVideoEffect != nullptr)
    {
        if (m_mrcAudioEffect != nullptr)
        {
            co_await m_mediaCapture.RemoveEffectAsync(m_mrcAudioEffect);
            m_mrcAudioEffect = nullptr;
        }

        if (m_mrcPreviewEffect != nullptr)
        {
            co_await m_mediaCapture.RemoveEffectAsync(m_mrcPreviewEffect);
            m_mrcPreviewEffect = nullptr;
        }

        if (m_mrcVideoEffect != nullptr)
        {
            co_await m_mediaCapture.RemoveEffectAsync(m_mrcVideoEffect);
            m_mrcVideoEffect = nullptr;
        }
    }
    else
    {
        co_return;
    }
}

hresult CaptureEngine::CreatePhotoTexture(uint32_t width, uint32_t height)
{
    auto resources = m_d3d11DeviceResources.lock();
    NULL_CHK_HR(resources, MF_E_UNEXPECTED);

    com_ptr<IDXGIAdapter> dxgiAdapter = nullptr;
    auto d3dDevice = resources->GetDevice();

    auto desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM, width, height);
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MipLevels = 1;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;

    com_ptr<ID3D11Texture2D> photoTexture = nullptr;
    IFR(d3dDevice->CreateTexture2D(&desc, nullptr, photoTexture.put()));

    auto srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(photoTexture.get(), D3D11_SRV_DIMENSION_TEXTURE2D);

    com_ptr<ID3D11ShaderResourceView> srv = nullptr;
    IFR(d3dDevice->CreateShaderResourceView(photoTexture.get(), &srvDesc, srv.put()));

    // create a media buffer for the texture
    com_ptr<IMFMediaBuffer> dxgiMediaBuffer = nullptr;
    IFR(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), photoTexture.get(), 0, /*fBottomUpWhenLinear*/false, dxgiMediaBuffer.put()));

    // create a sample with the buffer
    com_ptr<IMFSample> mediaSample = nullptr;
    IFR(MFCreateSample(mediaSample.put()));

    IFR(mediaSample->AddBuffer(dxgiMediaBuffer.get()));

    m_transform = Media::Transform().as<ITransformPriv>();
    m_photoTextureDesc = desc;
    m_photoTexture = photoTexture;
    m_photoTextureSRV = srv;
    m_photoSample = mediaSample;

    return S_OK;
}