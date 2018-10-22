#include "pch.h"
#include "Plugin.CaptureEngine.h"
#include "Media.Functions.h"
#include "Media.Capture.Payload.h"
#include "Media.Capture.MrcAudioEffect.h"
#include "Media.Capture.MrcVideoEffect.h"

#include <mferror.h>
#include <mfmediacapture.h>

using namespace winrt;
using namespace CameraCapture::Plugin::implementation;
using namespace CameraCapture::Media::Capture::implementation;
using namespace Windows::Foundation;
using namespace Windows::Media::Effects;
using namespace Windows::Media::Capture;
using namespace Windows::Media::MediaProperties;

using winrtCaptureEngine = CameraCapture::Plugin::CaptureEngine;

IAsyncOperation<boolean> WaitForCompletion(completion_source<boolean>& completion)
{
    co_return co_await completion;
}


CameraCapture::Plugin::IModule CaptureEngine::Create(
    _In_ std::weak_ptr<IUnityDeviceResource> const& unityDevice,
    _In_ StateChangedCallback fnCallback)
{
    auto capture = make<CaptureEngine>();

    if (SUCCEEDED(capture.as<IModulePriv>()->Initialize(unityDevice, fnCallback)))
    {
        return capture;
    }

    return nullptr;
}


CaptureEngine::CaptureEngine()
    : m_isShutdown(false)
    , m_stopCompletion()
    , m_startPreviewOp(nullptr)
    , m_stopPreviewOp(nullptr)
    , m_streamType(MediaStreamType::VideoPreview)
    , m_mediaCapture(nullptr)
    , m_failedEventToken()
    , m_mrcAudioEffect(nullptr)
    , m_mrcVideoEffect(nullptr)
    , m_mrcPreviewEffect(nullptr)
    , m_mediaSink(nullptr)
    , m_payloadHandler(nullptr)
    , m_audioSample(nullptr)
    , m_videoBuffer(nullptr)
    , m_appCoordinateSystem(nullptr)
{
}

void CaptureEngine::Shutdown()
{
    auto guard = slim_lock_guard(m_mutex);

    if (m_isShutdown)
    {
        return;
    }
    m_isShutdown = true;

    if (m_startPreviewOp != nullptr)
    {
        m_startPreviewOp.Cancel();

        m_startPreviewOp = nullptr;
    }

    if (m_stopPreviewOp != nullptr)
    {
        m_stopPreviewOp.Cancel();

        m_stopPreviewOp = nullptr;
    }

    if (m_mediaCapture != nullptr)
    {
        auto asyncWait = WaitForCompletion(m_stopCompletion);

        auto asyncStop = StopPreviewAsync();

        asyncWait.get();
    }

    ReleaseDeviceResources();

    Module::Shutdown();
}

HRESULT CaptureEngine::StartPreview(uint32_t width, uint32_t height, bool enableAudio, bool enableMrc)
{
    auto guard = slim_lock_guard(m_mutex);

    if (m_startPreviewOp != nullptr && m_startPreviewOp.Status() == AsyncStatus::Started)
    {
        IFR(MF_E_MULTIPLE_BEGIN);
    }

    if (m_mediaCapture != nullptr)
    {
        IFR(MF_E_ALREADY_INITIALIZED);
    }

    m_startPreviewOp = StartPreviewAsync(width, height, enableAudio, enableMrc);
    m_startPreviewOp.Completed([=](auto const& asyncOp, AsyncStatus const& status)
    {
        UNREFERENCED_PARAMETER(asyncOp);

        if (status == AsyncStatus::Canceled)
        {
            return;
        }
        else if (status == AsyncStatus::Error)
        {
            Failed();
        }
        else
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

HRESULT CaptureEngine::StopPreview()
{
    auto guard = slim_lock_guard(m_mutex);

    return S_OK;
}


// private
IAsyncAction CaptureEngine::StartPreviewAsync(
    uint32_t width, uint32_t height, 
    boolean enableAudio, boolean enableMrc)
{
    co_await resume_background();

    if (m_mediaCapture == nullptr)
    {
        co_await CreateMediaCaptureAsync(enableAudio, MediaCategory::Communications);
    }
    else
    {
        co_await RemoveMrcEffectsAsync();
    }

    // set video controller properties
    auto videoController = m_mediaCapture.VideoDeviceController();
    videoController.DesiredOptimization(Windows::Media::Devices::MediaCaptureOptimization::LatencyThenQuality);

    auto videoEncProps = GetVideoDeviceProperties(videoController, MediaStreamType::VideoRecord, width, height, MediaEncodingSubtypes::Nv12());
    co_await videoController.SetMediaStreamPropertiesAsync(MediaStreamType::VideoRecord, videoEncProps);

    auto captureSettings = m_mediaCapture.MediaCaptureSettings();
    if (captureSettings.VideoDeviceCharacteristic() != VideoDeviceCharacteristic::AllStreamsIdentical
        &&
        captureSettings.VideoDeviceCharacteristic() != VideoDeviceCharacteristic::PreviewRecordStreamsIdentical)
    {
        videoEncProps = GetVideoDeviceProperties(videoController, MediaStreamType::VideoPreview, width, height, MediaEncodingSubtypes::Nv12());
        co_await videoController.SetMediaStreamPropertiesAsync(MediaStreamType::VideoPreview, videoEncProps);
    }

    // encoding profile based on 720p
    auto encodingProfile = MediaEncodingProfile::CreateMp4(VideoEncodingQuality::HD720p);
    encodingProfile.Container(nullptr);

    if (!enableAudio)
    {
        encodingProfile.Audio(nullptr);
    }
    else
    {
        auto audioController = m_mediaCapture.AudioDeviceController();

        auto audioMediaProperties = audioController.GetMediaStreamProperties(MediaStreamType::Audio);

        auto audioMediaProperty = audioMediaProperties.as<IAudioEncodingProperties>();
        if (m_streamType == MediaStreamType::VideoPreview)
        {
            encodingProfile.Audio().Subtype(MediaEncodingSubtypes::Float());
        }
    }

    auto videoMediaProperty = videoController.GetMediaStreamProperties(m_streamType).as<IVideoEncodingProperties>();
    if (videoMediaProperty != nullptr)
    {
        if (m_streamType == MediaStreamType::VideoPreview)
        {
            encodingProfile.Video().Subtype(MediaEncodingSubtypes::Bgra8());
        }
        encodingProfile.Video().Width(videoMediaProperty.Width());
        encodingProfile.Video().Height(videoMediaProperty.Height());
    }

    // media sink
    auto mediaSink = make<Sink>(encodingProfile);

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

        auto previewFrame = co_await m_mediaCapture.GetPreviewFrameAsync();
    }

    // payload handler
    auto payloadHandler = make<PayloadHandler>();
    mediaSink.PayloadHandler(payloadHandler);

    // store locals
    m_mediaSink = mediaSink;
    m_payloadHandler = payloadHandler;

    // setup events
    m_failedEventToken = m_mediaCapture.Failed(([=](auto const& sender, MediaCaptureFailedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        CALLBACK_STATE state{};
        ZeroMemory(&state, sizeof(CALLBACK_STATE));

        state.type = CallbackType::Failed;

        ZeroMemory(&state.value.failedState, sizeof(FAILED_STATE));

        static const hstring errorMessege = args.Message();

        state.value.failedState.hresult = args.Code();

        Callback(state);
    }));

    m_sampleEventToken = m_payloadHandler.OnSample([this](auto const&, Media::Capture::Payload const& payload)
    {
        auto streamSample = payload.as<IStreamSample>();
        if (streamSample == nullptr)
        {
            return;
        }

        if (L"Audio" == payload.MediaEncodingProperties().Type())
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

            IFV(CopySample(MFMediaType_Audio, streamSample->Sample(), m_audioSample.get()));

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
        else if (L"Video" == payload.MediaEncodingProperties().Type())
        {
            boolean bufferChanged = false;

            auto videoProps = payload.MediaEncodingProperties().as<IVideoEncodingProperties>();

            if (m_videoBuffer == nullptr
                ||
                m_videoBuffer->frameTexture == nullptr
                ||
                m_videoBuffer->frameTextureDesc.Width != videoProps.Width()
                ||
                m_videoBuffer->frameTextureDesc.Height != videoProps.Height())
            {
                auto resources = m_deviceResources.lock();

                NULL_CHK_R(resources);

                com_ptr<ID3D11DeviceResource> spD3D11Resources = nullptr;
                IFV(resources->QueryInterface(__uuidof(ID3D11DeviceResource), spD3D11Resources.put_void()));

                // make sure we have created our own d3d device
                IFV(CreateDeviceResources());
                
                IFV(SharedTexture::Create(spD3D11Resources->GetDevice().get(), m_dxgiDeviceManager.get(), videoProps.Width(), videoProps.Height(), m_videoBuffer));

                bufferChanged = true;
            }

            // copy the data
            IFV(CopySample(MFMediaType_Video, streamSample->Sample(), m_videoBuffer->mediaSample.get()));

            // did the texture description change, if so, raise callback
            CALLBACK_STATE state{};
            ZeroMemory(&state, sizeof(CALLBACK_STATE));
            
            state.type = CallbackType::Capture;
            
            ZeroMemory(&state.value.captureState, sizeof(CAPTURE_STATE));

            state.value.captureState.stateType = CaptureStateType::PreviewVideoFrame;
            state.value.captureState.width = m_videoBuffer->frameTextureDesc.Width;
            state.value.captureState.height = m_videoBuffer->frameTextureDesc.Height;
            state.value.captureState.texturePtr = m_videoBuffer->frameTextureSRV.get();

            // if there is coordinate space information, raise callback
            if (m_appCoordinateSystem != nullptr)
            {
                if (SUCCEEDED(m_videoBuffer->UpdateTransforms(m_appCoordinateSystem)))
                {
                    state.value.captureState.worldMatrix = m_videoBuffer->cameraViewInWorldMatrix;
                    state.value.captureState.projectionMatrix = m_videoBuffer->cameraProjectionMatrix;

                    bufferChanged = true;
                }
            }
            
            if (bufferChanged)
            {
                Callback(state);
            }
        }
    });

    m_stopPreviewOp = nullptr;
    m_stopCompletion.reset();

    co_return;
}

IAsyncAction CaptureEngine::StopPreviewAsync()
{
    co_await resume_background();

    if (m_mediaSink != nullptr)
    {
        m_mediaSink.PayloadHandler(nullptr);
        m_mediaSink = nullptr;
    }

    if (m_payloadHandler != nullptr)
    {
        m_payloadHandler.OnSample(m_sampleEventToken);
        m_payloadHandler = nullptr;
    }

    if (m_mediaCapture != nullptr)
    {
        m_mediaCapture.Failed(m_failedEventToken);

        if (m_streamType == MediaStreamType::VideoRecord)
        {
            co_await m_mediaCapture.StopRecordAsync();
        }
        else if (m_streamType == MediaStreamType::VideoPreview)
        {
            co_await m_mediaCapture.StopPreviewAsync();
        }

        co_await ReleaseMediaCaptureAsync();
    }

    // complete trigger completion
    m_stopCompletion.set(true);

    co_return;
}


IAsyncAction CaptureEngine::CreateMediaCaptureAsync(
    boolean enableAudio,
    MediaCategory category)
{
    if (m_mediaCapture != nullptr)
    {
        co_return;
    }

    IFT(CreateDeviceResources());

    auto initSettings = MediaCaptureInitializationSettings();

    auto videoDevice = co_await GetFirstDeviceAsync(Windows::Devices::Enumeration::DeviceClass::VideoCapture);

    initSettings.VideoDeviceId(videoDevice.Id());

    if (enableAudio)
    {
        auto audioDevice = co_await GetFirstDeviceAsync(Windows::Devices::Enumeration::DeviceClass::AudioCapture);

        initSettings.AudioDeviceId(audioDevice.Id());
    }

    initSettings.MediaCategory(category);
    initSettings.MemoryPreference(MediaCaptureMemoryPreference::Auto);
    initSettings.PhotoCaptureSource(enableAudio ? PhotoCaptureSource::Auto : PhotoCaptureSource::VideoPreview);
    initSettings.StreamingCaptureMode(enableAudio ? StreamingCaptureMode::AudioAndVideo : StreamingCaptureMode::Video);
    initSettings.SharingMode(MediaCaptureSharingMode::ExclusiveControl);

    auto advancedInitSettings = initSettings.as<IAdvancedMediaCaptureInitializationSettings>();
    IFT(advancedInitSettings->SetDirectxDeviceManager(m_dxgiDeviceManager.get()));

    auto mediaCapture = Windows::Media::Capture::MediaCapture();

    co_await mediaCapture.InitializeAsync(initSettings);

    m_mediaCapture = mediaCapture;
}

IAsyncAction CaptureEngine::ReleaseMediaCaptureAsync()
{
    co_await resume_background();

    if (m_mediaCapture == nullptr)
    {
        co_return;
    }

    co_await RemoveMrcEffectsAsync();

    m_mediaCapture.Close();

    m_mediaCapture = nullptr;

    co_return;
}


HRESULT CaptureEngine::CreateDeviceResources()
{
    if (m_d3dDevice != nullptr && m_dxgiDeviceManager != nullptr)
    {
        return S_OK;
    }

    auto resources = m_deviceResources.lock();
    NULL_CHK_HR(resources, MF_E_UNEXPECTED);

    com_ptr<ID3D11DeviceResource> spD3D11Resources;
    IFR(resources->QueryInterface(__uuidof(ID3D11DeviceResource), spD3D11Resources.put_void()));

    com_ptr<IDXGIDevice> dxgiDevice = nullptr;
    IFR(spD3D11Resources->GetDevice()->QueryInterface(guid_of<IDXGIDevice>(), dxgiDevice.put_void()));

    com_ptr<IDXGIAdapter> dxgiAdapter = nullptr;
    IFR(dxgiDevice->GetAdapter(dxgiAdapter.put()));

    com_ptr<ID3D11Device> d3dDevice = nullptr;
    IFR(CreateMediaDevice(dxgiAdapter.get(), d3dDevice.put()));

    // create DXGIManager
    uint32_t resetToken;
    com_ptr<IMFDXGIDeviceManager> dxgiDeviceManager = nullptr;
    IFR(MFCreateDXGIDeviceManager(&resetToken, dxgiDeviceManager.put()));

    // associate device with dxgiManager
    IFR(dxgiDeviceManager->ResetDevice(d3dDevice.get(), resetToken));

    // success, store the values
    m_d3dDevice.attach(d3dDevice.detach());
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

    if (m_videoBuffer != nullptr)
    {
        m_videoBuffer->Reset();

        m_videoBuffer = nullptr;
    }

    if (m_dxgiDeviceManager != nullptr)
    {
        m_dxgiDeviceManager = nullptr;
    }

    if (m_d3dDevice != nullptr)
    {
        m_d3dDevice = nullptr;
    }
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
        auto mrcVideoEffect = make<MrcVideoEffect>().as<IVideoEffectDefinition>();
        if (captureSettings.VideoDeviceCharacteristic() == VideoDeviceCharacteristic::AllStreamsIdentical ||
            captureSettings.VideoDeviceCharacteristic() == VideoDeviceCharacteristic::PreviewRecordStreamsIdentical)
        {
            // This effect will modify both the preview and the record streams
            m_mrcVideoEffect = co_await m_mediaCapture.AddVideoEffectAsync(mrcVideoEffect, MediaStreamType::VideoRecord);
        }
        else
        {
            m_mrcVideoEffect = co_await m_mediaCapture.AddVideoEffectAsync(mrcVideoEffect, MediaStreamType::VideoRecord);
            m_mrcPreviewEffect = co_await m_mediaCapture.AddVideoEffectAsync(mrcVideoEffect, MediaStreamType::VideoPreview);
        }

        if (enableAudio)
        {
            auto mrcAudioEffect = make<MrcAudioEffect>().as<IAudioEffectDefinition>();

            m_mrcAudioEffect = co_await m_mediaCapture.AddAudioEffectAsync(mrcAudioEffect);
        }
    }
    catch (hresult_error const& e)
    {
        Log(L"failed to add Mrc effects to streams: %s", e.message().c_str());
    }
}

IAsyncAction CaptureEngine::RemoveMrcEffectsAsync()
{
    if (m_mediaCapture == nullptr)
    {
        co_return;
    }

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
