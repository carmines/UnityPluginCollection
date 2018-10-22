#include "pch.h"
#include "Media.Capture.Sink.h"

using namespace winrt;
using namespace CameraCapture::Media::Capture::implementation;
using namespace Windows::Media::MediaProperties;

using winrtStreamSink = CameraCapture::Media::Capture::StreamSink;

Sink::Sink(MediaEncodingProfile const& encodingProfile)
    : m_currentState(State::Ready)
    , m_streamSinks()
    , m_payloadHandler(nullptr)
{

    if (encodingProfile.Audio() != nullptr)
    {
        auto sinkStream = make<StreamSink>(AudioStreamId, encodingProfile.Audio().as<IMediaEncodingProperties>(), *this);
        m_streamSinks.emplace_front(sinkStream.as<IMFStreamSink>());
    }

    if (encodingProfile.Video() != nullptr)
    {
        com_ptr<IMFMediaType> mediaType = nullptr;
        IFT(MFCreateMediaTypeFromProperties(winrt::get_unknown(encodingProfile.Video()), mediaType.put()));

        uint8_t streamId = VideoStreamId;
        if (encodingProfile.Audio() == nullptr)
        {
            streamId = 0;
        }

        auto sinkStream = make<StreamSink>(streamId, encodingProfile.Video().as<IMediaEncodingProperties>(), *this);
        m_streamSinks.emplace_back(sinkStream.as<IMFStreamSink>());
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

    slim_shared_lock_guard const guard(m_lock);

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

    slim_lock_guard guard(m_lock);

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
        auto current = (*it);

        DWORD dwCurrId;
        IFR(current->GetIdentifier(&dwCurrId));
        if (dwCurrId > dwStreamSinkIdentifier)
        {
            break;
        }
    }

    auto sinkStream = make<StreamSink>(static_cast<uint8_t>(dwStreamSinkIdentifier), pMediaType, *this);
    m_streamSinks.emplace(it, sinkStream.as<IMFStreamSink>());

    sinkStream.as<IMFStreamSink>().copy_to(ppStreamSink);

    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::RemoveStreamSink(
    DWORD dwStreamSinkIdentifier)
{
    Log(L"Sink::RemoveStreamSink()\n");

    slim_lock_guard guard(m_lock);

    IFR(CheckShutdown());

    auto it = m_streamSinks.begin();
    auto itEnd = m_streamSinks.end();
    for (; it != itEnd; ++it)
    {
        auto current = (*it);

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
        DWORD dwId;
        if (SUCCEEDED(streamSink->GetIdentifier(&dwId)))
        {
            if (dwId == dwStreamSinkIdentifier)
            {
                streamSink.as<winrtStreamSink>().Shutdown();

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

    slim_shared_lock_guard const guard(m_lock);

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

    slim_shared_lock_guard const guard(m_lock);

    IFR(CheckShutdown());

    auto it = m_streamSinks.begin();
    std::advance(it, dwIndex);

    if (it == m_streamSinks.end())
    {
        IFR(MF_E_INVALIDINDEX);
    }

    (*it).copy_to(ppStreamSink);

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

    slim_shared_lock_guard const guard(m_lock);

    IFR(CheckShutdown());

    auto it = m_streamSinks.begin();
    auto itEnd = m_streamSinks.end();
    do
    {
        auto sink = *it;
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

    slim_lock_guard guard(m_lock);

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

    slim_shared_lock_guard const guard(m_lock);

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

    slim_lock_guard guard(m_lock);

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

    slim_lock_guard guard(m_lock);

    IFR(CheckShutdown());

    m_currentState = State::Started;

    for (auto&& sink : m_streamSinks)
    {
        IFR(sink.as<winrtStreamSink>().Start(hnsSystemTime, llClockStartOffset));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::OnClockStop(
    MFTIME hnsSystemTime)
{
    Log(L"Sink::OnClockStop()\n");

    UNREFERENCED_PARAMETER(hnsSystemTime);

    slim_lock_guard guard(m_lock);

    IFR(CheckShutdown());

    m_currentState = State::Stopped;

    for (auto&& sink : m_streamSinks)
    {
        IFR(sink.as<winrtStreamSink>().Stop());
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT Sink::OnClockPause(
    MFTIME hnsSystemTime)
{
    Log(L"Sink::OnClockPause()\n");

    UNREFERENCED_PARAMETER(hnsSystemTime);

    slim_lock_guard guard(m_lock);

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

    slim_lock_guard guard(m_lock);

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

    //slim_lock_guard guard(m_lock);

    //IFR(CheckShutdown());

    return S_OK;
}


// Media::Sink
_Use_decl_annotations_
HRESULT Sink::OnEndOfStream()
{
    winrt::com_ptr<IMFPresentationClock> spPresentationClock = nullptr;

    {
        slim_shared_lock_guard guard(m_lock);

        bool initiateshutdown = true;
        for (auto&& streamSink : m_streamSinks)
        {
            initiateshutdown &= streamSink.as<winrtStreamSink>().State() == State::Eos;
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

    for (auto&& streamSink : m_streamSinks)
    {
        streamSink.as<winrtStreamSink>().Shutdown();
    }
    m_streamSinks.clear();

    if (m_presentationClock != nullptr)
    {
        m_presentationClock->RemoveClockStateSink(this);

        m_presentationClock = nullptr;
    }
}
