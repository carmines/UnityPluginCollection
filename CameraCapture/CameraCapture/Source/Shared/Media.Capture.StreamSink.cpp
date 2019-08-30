// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Capture.StreamSink.h"
#include "Media.Capture.StreamSink.g.cpp"
#include "Media.Payload.h"
#include "Media.PayloadHandler.h"
#include "Media.Functions.h"

#include <winrt/windows.media.mediaproperties.h>

using namespace winrt;
using namespace CameraCapture::Media::Capture::implementation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Media::MediaProperties;


StreamSink::StreamSink(
    uint8_t index,
    IMediaEncodingProperties const& encodingProperties,
	CameraCapture::Media::Capture::Sink const& parent)
    : m_currentState(State::Ready)
    , m_streamIndex(index)
    , m_parentSink(parent)
    , m_encodingProperties(encodingProperties)
	, m_setDiscontinuity(false)
	, m_enableSampleRequests(true)
	, m_sampleRequests(0)
	, m_lastTimestamp(-1)
	, m_lastDecodeTime(-1)
{
    IFT(MFCreateMediaTypeFromProperties(winrt::get_unknown(m_encodingProperties), m_mediaType.put()));
    IFT(m_mediaType->GetGUID(MF_MT_MAJOR_TYPE, &m_guidMajorType));
    IFT(m_mediaType->GetGUID(MF_MT_SUBTYPE, &m_guidSubType));
    IFT(MFCreateEventQueue(m_eventQueue.put()));
}

StreamSink::StreamSink(
    uint8_t index,
    IMFMediaType* pMediaType,
	CameraCapture::Media::Capture::Sink const& parent)
    : m_streamIndex(index)
    , m_parentSink(parent)
	, m_setDiscontinuity(false)
	, m_enableSampleRequests(true)
	, m_sampleRequests(0)
	, m_lastTimestamp(-1)
	, m_lastDecodeTime(-1)
{
	m_mediaType.copy_from(pMediaType);
	IFT(pMediaType->GetGUID(MF_MT_MAJOR_TYPE, &m_guidMajorType));
    IFT(pMediaType->GetGUID(MF_MT_SUBTYPE, &m_guidSubType));
	IFT(MFCreatePropertiesFromMediaType(m_mediaType.get(), guid_of<IMediaEncodingProperties>(), put_abi(m_encodingProperties)));
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

	std::shared_lock<slim_mutex> slock(m_mutex);

    m_parentSink.as<IMFMediaSink>().copy_to(ppMediaSink);

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::GetIdentifier(
    DWORD *pdwIdentifier)
{
	std::shared_lock<slim_mutex> slock(m_mutex);

    *pdwIdentifier = m_streamIndex;

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::GetMediaTypeHandler(
    IMFMediaTypeHandler **ppHandler)
{
    Log(L"StreamSink::GetMediaTypeHandler().\n");

	std::shared_lock<slim_mutex> slock(m_mutex);

    NULL_CHK_HR(m_parentSink, MF_E_NOT_INITIALIZED);

    IFR(CheckShutdown());

    // This stream object acts as its own type handler, so we QI ourselves.
    return QueryInterface(IID_PPV_ARGS(ppHandler));
}

_Use_decl_annotations_
HRESULT StreamSink::Flush()
{
    Log(L"StreamSink::Flush().\n");

	std::shared_lock<slim_mutex> slock(m_mutex);

	m_enableSampleRequests = TRUE;
	m_sampleRequests = 0;
	m_lastTimestamp = -1;
	m_lastDecodeTime = -1;

	auto propSet = Windows::Media::MediaProperties::MediaPropertySet();
	propSet.Insert(MF_PAYLOAD_FLUSH, box_value<uint32_t>(1));

	auto handler = m_parentSink.PayloadHandler();
	if (handler != nullptr)
	{
		handler.QueueStreamMetadata(propSet);
	}

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
	HRESULT hr = S_OK;

	bool shouldDrop = false;

	std::shared_lock<slim_mutex> slock(m_mutex);

    IFG(CheckShutdown(), done);

    if (State() == State::Paused || State() == State::Stopped)
    {
        IFG(MF_E_INVALIDREQUEST, done);
    }

	hr = ShouldDropSample(pSample, &shouldDrop);

	if (!shouldDrop)
	{
		if (m_setDiscontinuity)
		{
			IFG(pSample->SetUINT32(MFSampleExtension_Discontinuity, TRUE), done);

			m_setDiscontinuity = false;
		}

		auto handler = m_parentSink.PayloadHandler().as<Media::implementation::PayloadHandler>();
		if (handler != nullptr)
		{
			com_ptr<IMFSample> spSample = nullptr;
			spSample.copy_from(pSample); //add ref

			handler->QueueMFSample(m_guidMajorType, m_mediaType, spSample);
		}
	}

done:
    if (State() == State::Started && SUCCEEDED(hr))
	{
        IFR(NotifyRequestSample());
    }

    return hr;
}

_Use_decl_annotations_
HRESULT StreamSink::PlaceMarker(
    MFSTREAMSINK_MARKER_TYPE eMarkerType,
    const PROPVARIANT * pvarMarkerValue,
    const PROPVARIANT * pvarContextValue)
{
	std::shared_lock<slim_mutex> slock(m_mutex);

    IFR(CheckShutdown());

    if (State() == State::Paused || State() == State::Stopped)
    {
        IFR(MF_E_INVALIDREQUEST);
    }

    if (eMarkerType == MFSTREAMSINK_MARKER_ENDOFSEGMENT)
    {
        State(State::Eos);

        if (m_parentSink != nullptr)
        {
            IFR(m_parentSink.OnEndOfStream());
        }
	}

	auto propSet = Windows::Media::MediaProperties::MediaPropertySet();

	propSet.Insert(MF_PAYLOAD_MARKER_TYPE, box_value<uint32_t>(eMarkerType));

    if (eMarkerType == MFSTREAMSINK_MARKER_TICK)
    {
        if (pvarMarkerValue != nullptr && pvarMarkerValue->vt == VT_I8)
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

			propSet.Insert(MF_PAYLOAD_MARKER_TICK, ConvertProperty(*pvarContextValue));
			propSet.Insert(MF_PAYLOAD_MARKER_TICK_TIMESTAMP, ConvertProperty(*pvarMarkerValue));
        }
    }

	HRESULT hr = S_OK;

	auto handler = m_parentSink.PayloadHandler();
	if (handler != nullptr)
	{
		handler.QueueStreamMetadata(propSet);
	}

    IFR(NotifyMarker(pvarContextValue));

    return hr;
}

// IMFMediaEventGenerator
_Use_decl_annotations_
HRESULT StreamSink::BeginGetEvent(
    IMFAsyncCallback *pCallback,
    ::IUnknown *punkState)
{
	std::shared_lock<slim_mutex> slk(m_eventMutex);

    return m_eventQueue->BeginGetEvent(pCallback, punkState);
}

_Use_decl_annotations_
HRESULT StreamSink::EndGetEvent(
    IMFAsyncResult *pResult,
    IMFMediaEvent **ppEvent)
{
	std::shared_lock<slim_mutex> slk(m_eventMutex);

    return m_eventQueue->EndGetEvent(pResult, ppEvent);
}

_Use_decl_annotations_
HRESULT StreamSink::GetEvent(
    DWORD dwFlags,
    IMFMediaEvent **ppEvent)
{
	std::shared_lock<slim_mutex> slk(m_eventMutex);

    return m_eventQueue->GetEvent(dwFlags, ppEvent);
}

_Use_decl_annotations_
HRESULT StreamSink::QueueEvent(
    MediaEventType met,
    REFGUID guidExtendedType,
    HRESULT hrStatus,
    const PROPVARIANT *pvValue)
{
	std::shared_lock<slim_mutex> slk(m_eventMutex);

    return m_eventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
}


// IMFMediaTypeHandler
_Use_decl_annotations_
HRESULT StreamSink::GetCurrentMediaType(
    IMFMediaType **ppMediaType)
{
	std::shared_lock<slim_mutex> slock(m_mutex);

    m_mediaType.copy_to(ppMediaType);

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::GetMajorType(
    GUID *pguidMajorType)
{
	std::shared_lock<slim_mutex> slock(m_mutex);

    *pguidMajorType = m_guidMajorType;

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::GetMediaTypeByIndex(
    DWORD dwIndex,
    IMFMediaType **ppType)
{
	std::shared_lock<slim_mutex> slock(m_mutex);

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

	std::lock_guard<slim_mutex> guard(m_mutex);

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
	IFR(MFCreatePropertiesFromMediaType(m_mediaType.get(), guid_of<IMediaEncodingProperties>(), put_abi(m_encodingProperties)));

	auto handler = m_parentSink.PayloadHandler();
	if (handler != nullptr)
    {
		handler.QueueStreamMediaDescription(m_encodingProperties);
    }

    return S_OK;
}

// StreamSink
_Use_decl_annotations_
HRESULT StreamSink::Start(int64_t systemTime, int64_t clockStartOffset)
{
    UNREFERENCED_PARAMETER(systemTime);

	std::lock_guard<slim_mutex> guard(m_mutex);

    IFR(CheckShutdown());

    m_clockStartOffset = clockStartOffset;

    State(State::Started);

	auto handler = m_parentSink.PayloadHandler();
	if (handler != nullptr)
	{
		handler.QueueStreamMediaDescription(m_encodingProperties);
	}

    IFR(NotifyStarted());

    IFR(NotifyRequestSample());

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::Stop()
{
	std::lock_guard<slim_mutex> guard(m_mutex);

    IFR(CheckShutdown());

    State(State::Stopped);

    IFR(NotifyStopped());

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::Shutdown()
{
	std::lock_guard<slim_mutex> guard(m_mutex);

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
    IFR(QueueEvent(MEStreamSinkStopped, GUID_NULL, S_OK, NULL));

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::NotifyMarker(const PROPVARIANT *pVarContextValue)
{
    IFR(QueueEvent(MEStreamSinkMarker, GUID_NULL, S_OK, pVarContextValue));

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::NotifyRequestSample()
{
	for (DWORD i = m_sampleRequests; i < m_cMaxSampleRequests; i++)
	{
		m_sampleRequests++;

		IFR(QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL));
	}

    return S_OK;
}

_Use_decl_annotations_
HRESULT StreamSink::ShouldDropSample(
	IMFSample* pSample,
	bool *pDrop)
{
	NULL_CHK_HR(pSample, E_POINTER);
	NULL_CHK_HR(pDrop, E_POINTER);

	*pDrop = false;

	HRESULT hr = S_OK;

	LONGLONG timestamp = 0;
	BOOL hasDecodeTime = false;
	LONGLONG decodeTime = 0;
	DWORD totalLength = 0;

	// did we get a sample, even without requesting one
	if (m_sampleRequests == 0 || State() == State::Eos)
	{
		*pDrop = true;

		IFG(MF_E_NOTACCEPTING, done);
	}

	m_sampleRequests--;

	IFG(pSample->GetSampleTime(&timestamp), done);

	// if this is the first sample, calculate the offset from start timestamp
	if (m_startTimeOffset == PRESENTATION_CURRENT_POSITION)
	{
		SetStartTimeOffset(timestamp);

		if (m_startTimeOffset > 0)
		{
			Log(L"first sample is not at 0, ts=%I64d (m_startTimeOffset) timestamp=%I64d\n", m_startTimeOffset, timestamp);
		}
	}

	if (FAILED(pSample->GetUINT64(MFSampleExtension_DecodeTimestamp, (QWORD*)&decodeTime)))
	{
		// No MFSampleExtension_DecodeTimestamp means DTS eaqual to PTS, using timestamp
		if (timestamp <= m_lastDecodeTime)
		{
			Log(L"received sample with past timestamp, ts=%I64d (timestamp) m_lastDecodeTime=%I64d; dropping\n", timestamp, m_lastDecodeTime);
			*pDrop = true;
		}
	}
	else
	{
		hasDecodeTime = true;
		Log(L"DTS exists, DTS=%I64d PTS=%I64d", decodeTime, timestamp);
		if (decodeTime <= m_lastDecodeTime)
		{
			Log(L"received sample with past timestamp, ts=%I64d (decodeTime) m_lastDecodeTime=%I64d; dropping\n", decodeTime, m_lastDecodeTime);
			*pDrop = true;
		}
	}

	IFG(pSample->GetTotalLength(&totalLength), done);
	if (0 == totalLength)
	{
		Log(L"received 0 length sample; dropping\n");
		*pDrop = true;
	}

	if (*pDrop)
	{
		m_setDiscontinuity = true;
	}
	else
	{
		m_lastTimestamp = timestamp;
		if (hasDecodeTime)
		{
			m_lastDecodeTime = decodeTime;
		}
		else
		{
			m_lastDecodeTime = timestamp;
		}
	}

done:
	return hr;
}
