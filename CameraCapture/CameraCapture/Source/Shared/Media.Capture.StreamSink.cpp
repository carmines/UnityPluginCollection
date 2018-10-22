// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Capture.StreamSink.h"
#include "Media.Capture.AudioPayload.h"
#include "Media.Capture.VideoPayload.h"

using namespace winrt;
using namespace CameraCapture::Media::Capture::implementation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Media::MediaProperties;

using winrtPayload = CameraCapture::Media::Capture::Payload;
using winrtSink = CameraCapture::Media::Capture::Sink;

StreamSink::StreamSink(
    uint8_t index,
    IMediaEncodingProperties const& encodingProperties,
    winrtSink const& parent)
    : m_currentState(State::Ready)
    , m_streamIndex(index)
    , m_parentSink(parent)
    , m_encodingProperties(encodingProperties)
{
    IFT(MFCreateMediaTypeFromProperties(winrt::get_unknown(encodingProperties), m_mediaType.put()));
    IFT(m_mediaType->GetGUID(MF_MT_MAJOR_TYPE, &m_guidMajorType));
    IFT(m_mediaType->GetGUID(MF_MT_SUBTYPE, &m_guidSubType));
    IFT(MFCreateEventQueue(m_eventQueue.put()));
}

StreamSink::StreamSink(
    uint8_t index,
    IMFMediaType* pMediaType,
    winrtSink const& parent)
    : m_streamIndex(index)
    , m_parentSink(parent)
{
    IFT(pMediaType->QueryInterface(__uuidof(IMFMediaType), m_mediaType.put_void()));
    IFT(pMediaType->GetGUID(MF_MT_MAJOR_TYPE, &m_guidMajorType));
    IFT(pMediaType->GetGUID(MF_MT_SUBTYPE, &m_guidSubType));
    IFT(MFCreateEventQueue(m_eventQueue.put()));
}

// IMediaExtension
void StreamSink::SetProperties(IPropertySet const& configuration)
{
    UNREFERENCED_PARAMETER(configuration);
}

// IMFStreamSink
_Use_decl_annotations_
HRESULT StreamSink::GetMediaSink(
    IMFMediaSink **ppMediaSink)
{
    Log(L"StreamSink::GetMediaSink().\n");

    slim_shared_lock_guard const guard(m_lock);

    m_parentSink.as<IMFMediaSink>().copy_to(ppMediaSink);

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::GetIdentifier(
    DWORD *pdwIdentifier)
{
    slim_shared_lock_guard const guard(m_lock);

    *pdwIdentifier = m_streamIndex;

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::GetMediaTypeHandler(
    IMFMediaTypeHandler **ppHandler)
{
    Log(L"StreamSink::GetMediaTypeHandler().\n");

    slim_shared_lock_guard const guard(m_lock);

    NULL_CHK_HR(m_parentSink, MF_E_NOT_INITIALIZED);

    IFR(CheckShutdown());

    // This stream object acts as its own type handler, so we QI ourselves.
    return QueryInterface(IID_PPV_ARGS(ppHandler));
}

_Use_decl_annotations_
HRESULT StreamSink::Flush()
{
    Log(L"StreamSink::Flush().\n");

    slim_shared_lock_guard const guard(m_lock);

    //auto payloadProcessor = m_parentMediaSink.as<MixedRemoteViewCompositor::Media::NetworkMediaSink>().PayloadProcessor();
    //if (payloadProcessor != nullptr)
    //{
    //    const uint32_t c_cPayloadHeader = sizeof(MixedRemoteViewCompositor::Network::PayloadHeader);
    //    const uint32_t c_cFlushhHeader = sizeof(MixedRemoteViewCompositor::Network::FlushHeader);
    //    const uint32_t c_cTotalBundleSize = c_cPayloadHeader + c_cFlushhHeader;

    //    // create databuffer large enough for payload type and media buffer header
    //    auto headerData = MixedRemoteViewCompositor::Network::DataBuffer(c_cTotalBundleSize);

    //    uint8_t* rawBuffer = nullptr;
    //    IFR(headerData.as<IBufferByteAccess>()->Buffer(&rawBuffer));

    //    auto payloadHeader = reinterpret_cast<MixedRemoteViewCompositor::Network::PayloadHeader*>(rawBuffer);
    //    payloadHeader->Type = MixedRemoteViewCompositor::Network::DataBundleType::Flush;
    //    payloadHeader->Size = c_cFlushhHeader;

    //    auto flushHeader = reinterpret_cast<MixedRemoteViewCompositor::Network::FlushHeader*>(rawBuffer + c_cPayloadHeader);
    //    flushHeader->StreamIndex = static_cast<uint32_t>(m_streamId);

    //    headerData.Length(c_cPayloadHeader + payloadHeader->Size);

    //    auto dataBundle = MixedRemoteViewCompositor::Network::DataBundle(payloadHeader->Type);
    //    dataBundle.AddBuffer(headerData);

    //    auto payload = MixedRemoteViewCompositor::Media::Payload(static_cast<uint32_t>(m_streamId), MixedRemoteViewCompositor::Media::PayloadType::Flush, dataBundle);

    //    IFR(payloadProcessor.QueuePayload(payload));
    //}

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::ProcessSample(
    IMFSample *pSample)
{
    slim_shared_lock_guard const guard(m_lock);

    IFR(CheckShutdown());

    if (State() == State::Paused || State() == State::Stopped)
    {
        IFR(MF_E_INVALIDREQUEST);
    }

    if (State() == State::Eos) //drop
    {
        return S_OK;
    }

    // if this is the first sample, calculate the offset from start timestamp
    if (m_startTimeOffset == PRESENTATION_CURRENT_POSITION)
    {
        LONGLONG firstSampleTime = 0;
        IFR(pSample->GetSampleTime(&firstSampleTime));

        SetStartTimeOffset(firstSampleTime);
    }

    if (m_startTimeOffset > 0)
    {
        LONGLONG sampleTime = 0;
        IFR(pSample->GetSampleTime(&sampleTime));

        if (sampleTime < m_clockStartOffset) //we do not process samples predating the clock start offset;
        {
            return S_OK;
        }

        LONGLONG correctedSampleTime = sampleTime - m_startTimeOffset;
        if (correctedSampleTime < 0)
        {
            correctedSampleTime = 0;
        }

        IFR(pSample->SetSampleTime(correctedSampleTime));
    }

    auto handler = m_parentSink.PayloadHandler();
    if (handler != nullptr)
    {
        winrtPayload payload = nullptr;
        if (m_guidMajorType == MFMediaType_Audio)
        {
            IFR(AudioPayload::Create(m_mediaType.get(), pSample, payload));
        }
        else if (m_guidMajorType == MFMediaType_Video)
        {
            IFR(VideoPayload::Create(m_mediaType.get(), pSample, payload));
        }

        IFR(handler.QueuePayload(payload));
    }

    if (State() == State::Started)
    {
        IFR(NotifyRequestSample());
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::PlaceMarker(
    MFSTREAMSINK_MARKER_TYPE eMarkerType,
    const PROPVARIANT * pvarMarkerValue,
    const PROPVARIANT * pvarContextValue)
{
    UNREFERENCED_PARAMETER(pvarMarkerValue);

    slim_shared_lock_guard const guard(m_lock);

    IFR(CheckShutdown());

    if (State() == State::Paused || State() == State::Stopped)
    {
        return MF_E_INVALIDREQUEST;
    }

    if (eMarkerType == MFSTREAMSINK_MARKER_TYPE::MFSTREAMSINK_MARKER_ENDOFSEGMENT)
    {
        State(State::Eos);

        if (m_parentSink != nullptr)
        {
            IFR(m_parentSink.OnEndOfStream());
        }
    }

    if (eMarkerType == MFSTREAMSINK_MARKER_TICK)
    {
        if (pvarMarkerValue != nullptr || pvarMarkerValue->vt == VT_I8)
        {
            LARGE_INTEGER timeStamp = { 0 };
            timeStamp = pvarMarkerValue->hVal;

            if (m_startTimeOffset == PRESENTATION_CURRENT_POSITION)
            {
                // set the offset based on a marker tick
                SetStartTimeOffset(timeStamp.QuadPart);
            }

            if (m_startTimeOffset > 0)
            {
                LONGLONG correctedSampleTime = timeStamp.QuadPart - m_startTimeOffset;
                if (correctedSampleTime < 0)
                    correctedSampleTime = 0;

                timeStamp.QuadPart = correctedSampleTime;
            }
        }
    }

    auto handler = m_parentSink.PayloadHandler();
    if (handler != nullptr)
    {
        winrtPayload payload = nullptr;

        //    auto marker = make<Marker>(m_streamId);
        //    auto markerPriv = marker.as<IMarkerPriv>();
        //    markerPriv->MarkerType(eMarkerType);
        //    markerPriv->MarkerValue(pvarMarkerValue);
        //    markerPriv->ContextValue(pvarContextValue);

        IFR(handler.QueuePayload(payload));
    }

    IFR(NotifyMarker(pvarContextValue));

    return S_OK;
}

// IMFMediaEventGenerator
_Use_decl_annotations_
HRESULT StreamSink::BeginGetEvent(
    IMFAsyncCallback *pCallback,
    ::IUnknown *punkState)
{
    slim_shared_lock_guard const guard(m_eventLock);

    return m_eventQueue->BeginGetEvent(pCallback, punkState);
}

_Use_decl_annotations_
HRESULT StreamSink::EndGetEvent(
    IMFAsyncResult *pResult,
    IMFMediaEvent **ppEvent)
{
    slim_shared_lock_guard const guard(m_eventLock);

    return m_eventQueue->EndGetEvent(pResult, ppEvent);
}

_Use_decl_annotations_
HRESULT StreamSink::GetEvent(
    DWORD dwFlags,
    IMFMediaEvent **ppEvent)
{
    slim_shared_lock_guard const guard(m_eventLock);

    return m_eventQueue->GetEvent(dwFlags, ppEvent);
}

_Use_decl_annotations_
HRESULT StreamSink::QueueEvent(
    MediaEventType met,
    REFGUID guidExtendedType,
    HRESULT hrStatus,
    const PROPVARIANT *pvValue)
{
    slim_shared_lock_guard const guard(m_eventLock);

    return m_eventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
}


// IMFMediaTypeHandler
_Use_decl_annotations_
HRESULT StreamSink::GetCurrentMediaType(
    IMFMediaType **ppMediaType)
{
    slim_shared_lock_guard const guard(m_lock);

    m_mediaType.copy_to(ppMediaType);

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::GetMajorType(
    GUID *pguidMajorType)
{
    slim_shared_lock_guard const guard(m_lock);

    *pguidMajorType = m_guidMajorType;

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::GetMediaTypeByIndex(
    DWORD dwIndex,
    IMFMediaType **ppType)
{
    slim_shared_lock_guard const guard(m_lock);

    // support only one mediatype
    if (dwIndex > 0)
    {
        IFR(MF_E_NO_MORE_TYPES);
    }

    m_mediaType.copy_to(ppType);

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::GetMediaTypeCount(
    DWORD *pdwTypeCount)
{
    *pdwTypeCount = 1;

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::IsMediaTypeSupported(
    IMFMediaType *pMediaType,
    IMFMediaType **ppMediaType)
{
    NULL_CHK_HR(pMediaType, E_INVALIDARG);

    if (ppMediaType != nullptr)
    {
        *ppMediaType = nullptr;
    }

    return VerifyMediaType(pMediaType);
}

_Use_decl_annotations_
HRESULT StreamSink::SetCurrentMediaType(
    IMFMediaType *pMediaType)
{
    NULL_CHK_HR(pMediaType, E_INVALIDARG);

    slim_lock_guard guard(m_lock);

    if (m_mediaType != nullptr)
    {
        IFR(IsMediaTypeSupported(pMediaType, nullptr));
    }

    com_ptr<IMFMediaType> mediaType = nullptr;
    IFR(MFCreateMediaType(mediaType.put()));
    IFR(pMediaType->CopyAllItems(mediaType.get()));
    IFR(mediaType->GetGUID(MF_MT_MAJOR_TYPE, &m_guidMajorType));
    IFR(mediaType->GetGUID(MF_MT_SUBTYPE, &m_guidSubType));

    m_mediaType = mediaType;

    //auto payloadProcessor = m_parentSink->PayloadProcessor();
    //if (payloadProcessor != nullptr)
    //{
    //    auto setMediaType = make<MediaType>(m_streamId);
    //    IFR(setMediaType.as<IMediaTypeInternal>()->SetMediaType(m_mediaType.get()));

    //    auto payload = MixedRemoteViewCompositor::Media::Payload(static_cast<uint32_t>(m_streamId), MixedRemoteViewCompositor::Media::PayloadType::SetMediaType, setMediaType);
    //    IFR(payloadProcessor.QueuePayload(payload));
    //}

    return S_OK;
}

// StreamSink
_Use_decl_annotations_
HRESULT StreamSink::Start(int64_t systemTime, int64_t clockStartOffset)
{
    UNREFERENCED_PARAMETER(systemTime);

    slim_lock_guard guard(m_lock);

    IFR(CheckShutdown());

    m_clockStartOffset = clockStartOffset;

    State(State::Started);

    IFR(NotifyStarted());

    IFR(NotifyRequestSample());

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::Stop()
{
    slim_lock_guard guard(m_lock);

    IFR(CheckShutdown());

    State(State::Stopped);

    IFR(NotifyStopped());

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::Shutdown()
{
    slim_lock_guard guard(m_lock);

    if (m_currentState == State::Shutdown)
    {
        return S_OK;
    }
    m_currentState = State::Shutdown;

    m_clockStartOffset = PRESENTATION_CURRENT_POSITION;
    m_startTimeOffset = PRESENTATION_CURRENT_POSITION;

    if (m_eventQueue != nullptr)
    {
        m_eventQueue->Shutdown();
    }

    m_eventQueue = nullptr;
    m_mediaType = nullptr;
    m_parentSink = nullptr;

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::VerifyMediaType(
    IMFMediaType* pMediaType) const
{
    GUID guidMajorType;
    IFR(pMediaType->GetGUID(MF_MT_MAJOR_TYPE, &guidMajorType));

    GUID guidSubType;
    IFR(pMediaType->GetGUID(MF_MT_SUBTYPE, &guidSubType));
    if ((guidMajorType == m_guidMajorType) && (guidSubType == m_guidSubType))
    {
        return S_OK;
    }
    else
    {
        return MF_E_INVALIDMEDIATYPE;
    }
}

_Use_decl_annotations_
HRESULT StreamSink::NotifyStarted()
{
    PROPVARIANT propVar;
    PropVariantInit(&propVar);
    propVar.vt = VT_EMPTY;
    IFR(QueueEvent(MEStreamSinkStarted, GUID_NULL, S_OK, &propVar));
    IFR(PropVariantClear(&propVar));

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::NotifyStopped()
{
    PROPVARIANT propVar;
    PropVariantInit(&propVar);
    propVar.vt = VT_EMPTY;
    IFR(QueueEvent(MEStreamSinkStopped, GUID_NULL, S_OK, &propVar));
    IFR(PropVariantClear(&propVar));

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::NotifyMarker(const PROPVARIANT *pVarContextValue)
{
    if (pVarContextValue == nullptr)
    {
        PROPVARIANT propVar;
        PropVariantInit(&propVar);
        propVar.vt = VT_EMPTY;
        IFR(QueueEvent(MEStreamSinkMarker, GUID_NULL, S_OK, &propVar));
        IFR(PropVariantClear(&propVar));
    }
    else
    {
        IFR(QueueEvent(MEStreamSinkMarker, GUID_NULL, S_OK, pVarContextValue));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::NotifyRequestSample()
{
    PROPVARIANT propVar;
    PropVariantInit(&propVar);
    propVar.vt = VT_EMPTY;
    IFR(QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, &propVar));
    IFR(PropVariantClear(&propVar));

    return S_OK;
}
