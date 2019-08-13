// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Plugin.CaptureEngine.h"
#include "Plugin.CaptureEngine.g.cpp"

#include "Media.Functions.h"
#include "Media.Capture.Payload.h"
#include "Media.Capture.MrcAudioEffect.h"
#include "Media.Capture.MrcVideoEffect.h"

#include <mferror.h>
#include <mfmediacapture.h>

#include <pplawait.h>

using namespace winrt;
using namespace CameraCapture::Plugin::implementation;
using namespace CameraCapture::Media::Capture::implementation;
using namespace Windows::Foundation;
using namespace Windows::Media::Effects;
using namespace Windows::Media::Capture;
using namespace Windows::Media::MediaProperties;

using winrtCaptureEngine = CameraCapture::Plugin::CaptureEngine;

_Use_decl_annotations_
CameraCapture::Plugin::IModule CaptureEngine::Create(
    std::weak_ptr<IUnityDeviceResource> const& unityDevice,
    StateChangedCallback fnCallback,
    void* pCallbackObject)
{
    auto capture = make<CaptureEngine>();

    if (SUCCEEDED(capture.as<IModulePriv>()->Initialize(unityDevice, fnCallback, pCallbackObject)))
    {
        return capture;
    }

    return nullptr;
}


CaptureEngine::CaptureEngine()
    : m_isShutdown(false)
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
        concurrency::create_task([=]
        {
            try
            {
                StopPreviewCoroutine().get();
            }
            catch (...)
            {
            }
        }).get();
    }

    ReleaseDeviceResources();

    Module::Shutdown();
}

HRESULT CaptureEngine::StartPreview(uint32_t width, uint32_t height, bool enableAudio, bool enableMrc)
{
    auto guard = slim_lock_guard(m_mutex);

    if (m_mediaCapture != nullptr)
    {
        IFR(MF_E_ALREADY_INITIALIZED);
    }

    if (m_startPreviewOp != nullptr && m_startPreviewOp.Status() == AsyncStatus::Started)
    {
        IFR(MF_E_MULTIPLE_BEGIN);
    }

    m_startPreviewOp = StartPreviewCoroutine(width, height, enableAudio, enableMrc);
    m_startPreviewOp.Completed([=](auto const asyncOp, AsyncStatus const status)
    {
        UNREFERENCED_PARAMETER(asyncOp);

        if (status == AsyncStatus::Error)
        {
            Failed();
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

        m_startPreviewOp = nullptr;
    });

    return S_OK;
}

HRESULT CaptureEngine::StopPreview()
{
    auto guard = slim_lock_guard(m_mutex);

    NULL_CHK_HR(m_mediaCapture, S_OK);

    if (m_stopPreviewOp != nullptr && m_stopPreviewOp.Status() == AsyncStatus::Started)
    {
        IFR(MF_E_MULTIPLE_BEGIN);
    }

    m_stopPreviewOp = StopPreviewCoroutine();
    m_stopPreviewOp.Completed([=](auto const asyncOp, AsyncStatus const status)
    {
        UNREFERENCED_PARAMETER(asyncOp);

        if (status == AsyncStatus::Error)
        {
            Failed();
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

        m_stopPreviewOp = nullptr;
    });

    return S_OK;
}

// private
HRESULT CaptureEngine::CreateDeviceResources()
{
    if (m_d3dDevice != nullptr && m_dxgiDeviceManager != nullptr)
    {
        return S_OK;
    }

    auto resources = m_d3d11DeviceResources.lock();
    NULL_CHK_HR(resources, MF_E_UNEXPECTED);

	com_ptr<IDXGIDevice> dxgiDevice = nullptr;
	IFR(resources->GetDevice()->QueryInterface(guid_of<IDXGIDevice>(), dxgiDevice.put_void()));

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


IAsyncAction CaptureEngine::StartPreviewCoroutine(
    uint32_t const width, uint32_t const height,
    boolean const enableAudio, boolean const enableMrc)
{
    // make sure this is on the calling thread
    IFT(CreateDeviceResources());

    winrt::apartment_context calling_thread;

    co_await resume_background();

    if (m_mediaCapture == nullptr)
    {
        co_await CreateMediaCaptureAsync(enableAudio, width, height);
    }
    else
    {
        co_await RemoveMrcEffectsAsync();
    }

	// set video controller properties
	auto videoController = m_mediaCapture.VideoDeviceController();
	videoController.DesiredOptimization(Windows::Media::Devices::MediaCaptureOptimization::LatencyThenQuality);

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

	std::lock_guard<slim_mutex> guard(m_mutex);

    // store locals
    m_mediaSink = mediaSink;
    m_payloadHandler = payloadHandler;

    co_await calling_thread; // switch back to calling context

    m_sampleEventToken = m_payloadHandler.OnSample([this](auto const& sender, Media::Capture::Payload const& payload)
    {
        UNREFERENCED_PARAMETER(sender);

		std::shared_lock<slim_mutex> slock(m_mutex);

        if (m_isShutdown)
        {
            return;
        }
		
		if (payload == nullptr)
		{
			return;
		}

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
				auto resources = m_d3d11DeviceResources.lock();
				NULL_CHK_R(resources);

				// make sure we have created our own d3d device
				IFV(CreateDeviceResources());

				IFV(SharedTexture::Create(resources->GetDevice().get(), m_dxgiDeviceManager.get(), videoProps.Width(), videoProps.Height(), m_videoBuffer));

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
                    state.value.captureState.worldMatrix = m_videoBuffer->cameraToWorldTransform;
                    state.value.captureState.projectionMatrix = m_videoBuffer->cameraProjectionMatrix;

                    bufferChanged = true;
                }
            }
            
            if (bufferChanged)
            {
                Callback(state);
            }
        }
		else if (L"Container" == payload.MediaEncodingProperties().Type())
		{

		}
    });

	co_return;
 }

IAsyncAction CaptureEngine::StopPreviewCoroutine()
{
    winrt::apartment_context calling_thread;

    // callee should have a lock on the mutex
    if (m_payloadHandler != nullptr)
    {
        m_payloadHandler.OnSample(m_sampleEventToken);
    }

    co_await resume_background();

    if (m_mediaSink != nullptr)
    {
        m_mediaSink.PayloadHandler(nullptr);
        m_mediaSink = nullptr;
    }

    if (m_payloadHandler != nullptr)
    {
        m_payloadHandler = nullptr;
    }

    if (m_mediaCapture != nullptr)
    {
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

    co_await calling_thread; // switch back to calling context

	co_return;
}


IAsyncAction CaptureEngine::CreateMediaCaptureAsync(
	boolean const& enableAudio, 
	uint32_t const& width, 
	uint32_t const& height)
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
	}
	else
	{
		initSettings.SharingMode(MediaCaptureSharingMode::ExclusiveControl);
	}

    auto mediaCapture = Windows::Media::Capture::MediaCapture();
    co_await mediaCapture.InitializeAsync(initSettings);

    m_mediaCapture = mediaCapture;
	m_initSettings = initSettings;

	co_return;
}

IAsyncAction CaptureEngine::ReleaseMediaCaptureAsync()
{
    if (m_mediaCapture == nullptr)
    {
        co_return;
    }

    co_await RemoveMrcEffectsAsync();

    m_mediaCapture.Close();

    m_mediaCapture = nullptr;

	co_return;
}


IAsyncAction CaptureEngine::AddMrcEffectsAsync(
    boolean const enableAudio)
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

    co_return;
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

    co_return;
}
