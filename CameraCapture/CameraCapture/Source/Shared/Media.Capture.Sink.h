#pragma once

#include "Media/Capture/Sink.g.h"
#include "Media.Capture.StreamSink.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>

static constexpr uint8_t AudioStreamId = 0;
static constexpr uint8_t VideoStreamId = 1;

namespace winrt::CameraCapture::Media::Capture::implementation
{
    struct Sink : SinkT<Sink, IMFMediaSink, IMFClockStateSink>
    {
        Sink() = delete;
        Sink(Windows::Media::MediaProperties::MediaEncodingProfile const& encodingProfile);
        ~Sink() { Reset(); }

        // IMediaExtension
        void SetProperties(Windows::Foundation::Collections::IPropertySet const& configuration);

        // IMFMediaSink
        STDOVERRIDEMETHODIMP GetCharacteristics(
            _Out_ DWORD *pdwCharacteristics);
        STDOVERRIDEMETHODIMP AddStreamSink(
            _In_ DWORD dwStreamSinkIdentifier,
            _In_opt_ IMFMediaType *pMediaType,
            _COM_Outptr_opt_ IMFStreamSink **ppStreamSink);
        STDOVERRIDEMETHODIMP RemoveStreamSink(
            _In_ DWORD dwStreamSinkIdentifier);
        STDOVERRIDEMETHODIMP GetStreamSinkCount(
            _Out_ DWORD *pcStreamSinkCount);
        STDOVERRIDEMETHODIMP GetStreamSinkByIndex(
            _In_ DWORD dwIndex,
            _COM_Outptr_opt_ IMFStreamSink **ppStreamSink);
        STDOVERRIDEMETHODIMP GetStreamSinkById(
            _In_ DWORD dwStreamSinkIdentifier,
            _COM_Outptr_opt_ IMFStreamSink **ppStreamSink);
        STDOVERRIDEMETHODIMP SetPresentationClock(
            _In_opt_ IMFPresentationClock *pPresentationClock);
        STDOVERRIDEMETHODIMP GetPresentationClock(
            _COM_Outptr_opt_ IMFPresentationClock **ppPresentationClock);
        STDOVERRIDEMETHODIMP Shutdown();

        // IMFClockStateSink
        STDOVERRIDEMETHODIMP OnClockStart(
            _In_ MFTIME hnsSystemTime,
            _In_ LONGLONG llClockStartOffset);
        STDOVERRIDEMETHODIMP OnClockStop(
            _In_ MFTIME hnsSystemTime);
        STDOVERRIDEMETHODIMP OnClockPause(
            _In_ MFTIME hnsSystemTime);
        STDOVERRIDEMETHODIMP OnClockRestart(
            _In_ MFTIME hnsSystemTime);
        STDOVERRIDEMETHODIMP OnClockSetRate(
            _In_ MFTIME hnsSystemTime,
            _In_ float flRate);

        // Sink
        HRESULT OnEndOfStream();

        Capture::State State() { return m_currentState; }
        Capture::PayloadHandler PayloadHandler() { return m_payloadHandler; }
        void PayloadHandler(Capture::PayloadHandler const& value) { m_payloadHandler = value; }

    private:
        void Reset();

        inline bool IsState(Capture::State state)
        {
            return m_currentState == state;
        }

        inline void SetState(Capture::State state)
        {
            for (auto&& streamSink : m_streamSinks)
            {
                streamSink.as<Capture::StreamSink>().State(state);
            }
            m_currentState = state;
        }

        STDMETHODIMP CheckShutdown()
        {
            return !IsState(State::Shutdown) ? S_OK : MF_E_SHUTDOWN;
        }

    private:
        slim_mutex m_lock;
        std::atomic<Capture::State> m_currentState;

        std::list<com_ptr<IMFStreamSink>> m_streamSinks;
        com_ptr<IMFPresentationClock> m_presentationClock;

        Capture::PayloadHandler m_payloadHandler;
    };
}

namespace winrt::CameraCapture::Media::Capture::factory_implementation
{
    struct Sink : SinkT<Sink, implementation::Sink>
    {
    };
}
