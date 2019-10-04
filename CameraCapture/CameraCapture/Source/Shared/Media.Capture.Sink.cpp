// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Capture.Sink.h"
#include "Media.Capture.Sink.g.cpp"
#include "Media.PayloadHandler.g.h"

using namespace winrt;
using namespace CameraCapture::Media::Capture::implementation;
using namespace Windows::Media::MediaProperties;

using winrtStreamSink = CameraCapture::Media::Capture::StreamSink;

Sink::Sink(MediaEncodingProfile const& encodingProfile)
    : m_currentState(State::Ready)
    , m_mediaEncodingProfile(encodingProfile)
    , m_streamSinks()
    , m_payloadHandler(nullptr)
{

    if (encodingProfile.Audio() != nullptr)
    {
        auto sinkStream = CameraCapture::Media::Capture::StreamSink(AudioStreamId, encodingProfile.Audio(), *this);
        m_streamSinks.emplace_front(sinkStream);
    }

    if (encodingProfile.Video() != nullptr)
    {
        uint8_t streamId = VideoStreamId;
        if (encodingProfile.Audio() == nullptr)
        {
            streamId = 0;
        }

        auto sinkStream = CameraCapture::Media::Capture::StreamSink(streamId, encodingProfile.Video(), *this);
        m_streamSinks.emplace_back(sinkStream);
    }
}

// IMediaExtension
void Sink::SetProperties(Windows::Foundation::Collections::IPropertySet const& configuration)
{
    UNREFERENCED_PARAMETER(configuration);
}

// IMFMediaSink
_Use_decl_annotations_
HRESULT Sink::GetCharacteristics(
    DWORD *pdwCharacteristics)
{
    Log(L"Sink::GetCharacteristics()\n");

    auto guard = m_cs.Guard();

    IFR(CheckShutdown());

    *pdwCharacteristics = MEDIASINK_RATELESS | MEDIASINK_FIXED_STREAMS;

    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::AddStreamSink(
    DWORD dwStreamSinkIdentifier,
    IMFMediaType *pMediaType,
    IMFStreamSink **ppStreamSink)
{
    Log(L"Sink::AddStreamSink()\n");

    NULL_CHK_HR(pMediaType, E_INVALIDARG);
    NULL_CHK_HR(ppStreamSink, E_POINTER);

    *ppStreamSink = nullptr;

    auto guard = m_cs.Guard();

    IFR(CheckShutdown());

    com_ptr<IMFStreamSink> streamSink = nullptr;
    if (SUCCEEDED(GetStreamSinkById(dwStreamSinkIdentifier, streamSink.put())))
    {
        IFR(MF_E_STREAMSINK_EXISTS);
    }

    auto it = m_streamSinks.begin();
    auto itEnd = m_streamSinks.end();
    for (; it != itEnd; ++it)
    {
        auto current = (*it).as<IMFStreamSink>();

        DWORD dwCurrId;
        IFR(current->GetIdentifier(&dwCurrId));
        if (dwCurrId > dwStreamSinkIdentifier)
        {
            break;
        }
    }

    Windows::Media::MediaProperties::IMediaEncodingProperties encodingProperties = nullptr;
    IFR(MFCreatePropertiesFromMediaType(pMediaType, guid_of<IMediaEncodingProperties>(), put_abi(encodingProperties)));

    auto sinkStream = CameraCapture::Media::Capture::StreamSink(static_cast<uint8_t>(dwStreamSinkIdentifier), encodingProperties, *this);
    m_streamSinks.emplace(it, sinkStream);

    if (encodingProperties.Type() == L"Audio")
    {
        m_mediaEncodingProfile.Audio(encodingProperties.as<AudioEncodingProperties>());
    }
    else if(encodingProperties.Type() == L"Video")
    {
        m_mediaEncodingProfile.Video(encodingProperties.as<VideoEncodingProperties>());
    }

    sinkStream.as<IMFStreamSink>().copy_to(ppStreamSink);

    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::RemoveStreamSink(
    DWORD dwStreamSinkIdentifier)
{
    Log(L"Sink::RemoveStreamSink()\n");

    auto guard = m_cs.Guard();

    IFR(CheckShutdown());

    auto it = m_streamSinks.begin();
    auto itEnd = m_streamSinks.end();
    for (; it != itEnd; ++it)
    {
        auto current = (*it).as<IMFStreamSink>();

        DWORD dwId;
        IFR(current->GetIdentifier(&dwId));
        if (dwId == dwStreamSinkIdentifier)
        {
            break;
        }
    }

    if (it == itEnd)
    {
        IFR(MF_E_INVALIDSTREAMNUMBER);
    }

    m_streamSinks.remove_if([&](auto const& streamSink) -> bool
    {
        auto mfStreamSink = streamSink.try_as<IMFStreamSink>();

        DWORD dwId;
        if (mfStreamSink != nullptr && SUCCEEDED(mfStreamSink->GetIdentifier(&dwId)))
        {
            if (dwId == dwStreamSinkIdentifier)
            {
                streamSink.Shutdown();

                return true;
            }
        }

        return false;
    });

    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::GetStreamSinkCount(
    DWORD *pdwStreamSinkCount)
{
    Log(L"Sink::GetStreamSinkCount()\n");

    NULL_CHK_HR(pdwStreamSinkCount, E_POINTER);

    auto guard = m_cs.Guard();

    IFR(CheckShutdown());

    *pdwStreamSinkCount = static_cast<DWORD>(m_streamSinks.size());

    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::GetStreamSinkByIndex(
    DWORD dwIndex,
    IMFStreamSink **ppStreamSink)
{
    Log(L"Sink::GetStreamSinkByIndex()\n");

    NULL_CHK_HR(ppStreamSink, E_POINTER);

    *ppStreamSink = nullptr;

    auto guard = m_cs.Guard();

    IFR(CheckShutdown());

    auto it = m_streamSinks.begin();
    std::advance(it, dwIndex);

    if (it == m_streamSinks.end())
    {
        IFR(MF_E_INVALIDINDEX);
    }

    (*it).as<IMFStreamSink>().copy_to(ppStreamSink);

    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::GetStreamSinkById(
    DWORD dwStreamSinkIdentifier,
    IMFStreamSink **ppStreamSink)
{
    Log(L"Sink::GetStreamSinkById()\n");

    NULL_CHK_HR(ppStreamSink, E_POINTER);

    *ppStreamSink = nullptr;

    auto guard = m_cs.Guard();

    IFR(CheckShutdown());

    auto it = m_streamSinks.begin();
    auto itEnd = m_streamSinks.end();
    do
    {
        auto sink = (*it).as<IMFStreamSink>();

        DWORD dwStreamId;
        IFR(sink->GetIdentifier(&dwStreamId));

        if (dwStreamId == dwStreamSinkIdentifier)
        {
            sink.copy_to(ppStreamSink);
            break;
        }
    } while (++it != itEnd);

    if (it == itEnd)
    {
        IFR(MF_E_INVALIDSTREAMNUMBER);
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::SetPresentationClock(
    IMFPresentationClock *pPresentationClock)
{
    Log(L"Sink::SetPresentationClock()\n");

    NULL_CHK_HR(pPresentationClock, E_INVALIDARG);

    auto guard = m_cs.Guard();

    IFR(CheckShutdown());

    if (m_presentationClock != nullptr)
    {
        IFR(m_presentationClock->RemoveClockStateSink(this));
        m_presentationClock = nullptr;
    }

    IFR(pPresentationClock->AddClockStateSink(this));

    m_presentationClock.copy_from(pPresentationClock);

    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::GetPresentationClock(
    IMFPresentationClock **ppPresentationClock)
{
    Log(L"Sink::GetPresentationClock()\n");

    NULL_CHK_HR(ppPresentationClock, E_POINTER);

    *ppPresentationClock = nullptr;

    auto guard = m_cs.Guard();

    IFR(CheckShutdown());

    if (m_presentationClock != nullptr)
    {
        m_presentationClock.copy_to(ppPresentationClock);
    }
    
    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::Shutdown()
{
    Log(L"Sink::Shutdown()\n");

    auto guard = m_cs.Guard();

    Reset();

    return S_OK;
}


// IMFClockStateSink
_Use_decl_annotations_
HRESULT Sink::OnClockStart(
    MFTIME hnsSystemTime,
    LONGLONG llClockStartOffset)
{
    Log(L"Sink::OnClockStart()\n");

    auto guard = m_cs.Guard();

    IFR(CheckShutdown());

    m_currentState = State::Started;

    for (auto&& streamSink : m_streamSinks)
    {
        IFR(streamSink.Start(hnsSystemTime, llClockStartOffset));
    }

    QueueEncodingProfile(m_mediaEncodingProfile);
    
    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::OnClockStop(
    MFTIME hnsSystemTime)
{
    Log(L"Sink::OnClockStop()\n");

    UNREFERENCED_PARAMETER(hnsSystemTime);

    auto guard = m_cs.Guard();

    IFR(CheckShutdown());

    m_currentState = State::Stopped;

    for (auto&& sink : m_streamSinks)
    {
        IFR(sink.Stop());
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::OnClockPause(
    MFTIME hnsSystemTime)
{
    Log(L"Sink::OnClockPause()\n");

    UNREFERENCED_PARAMETER(hnsSystemTime);

    auto guard = m_cs.Guard();

    IFR(CheckShutdown());

    m_currentState = State::Paused;

    //for (auto&& sink : m_streamSinks)
    //{
    //    IFR(sink.as<Media::StreamSink>().Pause());
    //}

    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::OnClockRestart(
    MFTIME hnsSystemTime)
{
    Log(L"Sink::OnClockRestart()\n");

    UNREFERENCED_PARAMETER(hnsSystemTime);

    auto guard = m_cs.Guard();

    IFR(CheckShutdown());

    m_currentState = State::Started;

    //for (auto&& sink : m_streamSinks)
    //{
    //    IFR(sink.as<Media::StreamSink>().Restart());
    //}

    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::OnClockSetRate(
    MFTIME hnsSystemTime,
    float flRate)
{
    Log(L"Sink::OnClockSetRate()\n");

    UNREFERENCED_PARAMETER(hnsSystemTime);
    UNREFERENCED_PARAMETER(flRate);

    auto guard = m_cs.Guard();

    IFR(CheckShutdown());

    return S_OK;
}


// Media::Sink
_Use_decl_annotations_
HRESULT Sink::OnEndOfStream()
{
    winrt::com_ptr<IMFPresentationClock> spPresentationClock = nullptr;

    {
        auto guard = m_cs.Guard();

        bool initiateshutdown = true;
        for (auto&& streamSink : m_streamSinks)
        {
            initiateshutdown &= streamSink.State() == State::Eos;
        }

        if (initiateshutdown)
        {
            spPresentationClock = m_presentationClock;
        }
    }

    if (spPresentationClock != nullptr)
    {
        MFCLOCK_STATE state = MFCLOCK_STATE::MFCLOCK_STATE_INVALID;
        IFR(spPresentationClock->GetState(0, &state));
        if (state != MFCLOCK_STATE::MFCLOCK_STATE_INVALID)
        {
            spPresentationClock->Stop();
        }
    }

    return S_OK;
}

_Use_decl_annotations_
void Sink::Reset()
{
    if (m_currentState == State::Shutdown)
    {
        return;
    }
    m_currentState = State::Shutdown;

    if (m_payloadHandler != nullptr)
    {
        m_payloadHandler = nullptr;
    }

    while (m_streamSinks.size() > 0)
    {
        m_streamSinks.front().Shutdown();
        m_streamSinks.pop_front();
    }
    m_streamSinks.clear();

    if (m_presentationClock != nullptr)
    {
        m_presentationClock->RemoveClockStateSink(this);

        m_presentationClock = nullptr;
    }
}

// IPayloadHandler
void Sink::QueueEncodingProfile(MediaEncodingProfile const& mediaProfile)
{
    auto guard = m_cs.Guard();

    if (m_payloadHandler != nullptr)
    {
        m_payloadHandler.QueueEncodingProfile(mediaProfile);
    }
}

void Sink::QueueMetadata(MediaPropertySet const& metaData)
{
    auto guard = m_cs.Guard();

    if (m_payloadHandler != nullptr)
    {
        m_payloadHandler.QueueMetadata(metaData);
    }
}

void Sink::QueueEncodingProperties(IMediaEncodingProperties const& mediaDescription)
{
    auto guard = m_cs.Guard();

    if (m_payloadHandler != nullptr)
    {
        m_payloadHandler.QueueEncodingProperties(mediaDescription);
    }
}

void Sink::QueuePayload(CameraCapture::Media::Payload const& payload)
{
    auto guard = m_cs.Guard();

    if (m_payloadHandler != nullptr)
    {
        m_payloadHandler.QueuePayload(payload);
    }
}
