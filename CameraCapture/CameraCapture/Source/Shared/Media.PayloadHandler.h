// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Media.PayloadHandler.g.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>

#include "Media.Transform.h"

namespace winrt::CameraCapture::Media::implementation
{
    struct PayloadHandler : PayloadHandlerT<PayloadHandler, IMFAsyncCallback>
    {
        PayloadHandler();
        ~PayloadHandler() { Close(); }

        // PayloadHandler
        bool ProceesTranform(CameraCapture::Media::Payload const& payload);
        Windows::Perception::Spatial::SpatialCoordinateSystem AppCoordinateSystem();
        void AppCoordinateSystem(Windows::Perception::Spatial::SpatialCoordinateSystem const& value);

        // IClosable
        void Close();

        // IPayloadHandler
        void QueueEncodingProfile(Windows::Media::MediaProperties::MediaEncodingProfile const& mediaProfile);
        void QueueMetadata(Windows::Media::MediaProperties::MediaPropertySet const& metaData);
        void QueueEncodingProperties(Windows::Media::MediaProperties::IMediaEncodingProperties const& mediaDescription);
        void QueuePayload(CameraCapture::Media::Payload const& payload);

        winrt::event_token OnMediaProfile(Windows::Foundation::EventHandler<Windows::Media::MediaProperties::MediaEncodingProfile> const& handler)
        {
            return m_profileEvent.add(handler);
        }
        void OnMediaProfile(winrt::event_token const& token) noexcept
        {
            m_profileEvent.remove(token);
        }

        winrt::event_token OnStreamPayload(Windows::Foundation::EventHandler<CameraCapture::Media::Payload> const& handler)
        {
            return m_payloadEvent.add(handler);
        }
        void OnStreamPayload(winrt::event_token const& token)
        {
            m_payloadEvent.remove(token);
        }

        winrt::event_token OnStreamSample(Windows::Foundation::EventHandler<Windows::Media::Core::MediaStreamSample> const& handler)
        {
            return m_streamSampleEvent.add(handler);
        }
        void OnStreamSample(winrt::event_token const& token) noexcept
        {
            m_streamSampleEvent.remove(token);
        }

        winrt::event_token OnStreamMetadata(Windows::Foundation::EventHandler<Windows::Media::MediaProperties::MediaPropertySet> const& handler)
        {
            return m_metaDataEvent.add(handler);
        }
        void OnStreamMetadata(winrt::event_token const& token) noexcept
        {
            m_metaDataEvent.remove(token);
        }

        winrt::event_token OnStreamDescription(Windows::Foundation::EventHandler<Windows::Media::MediaProperties::IMediaEncodingProperties> const& handler)
        {
            return m_mediaDescriptionEvent.add(handler);
        }
        void OnStreamDescription(winrt::event_token const& token) noexcept
        {
            m_mediaDescriptionEvent.remove(token);
        }

        // internal
        STDMETHODIMP QueueMFSample(
            _In_ GUID majorType,
            _In_ com_ptr<IMFMediaType> const& type,
            _In_ com_ptr<IMFSample> const& sample);

        STDMETHODIMP QueueUnknown(
            _In_ ::IUnknown* pUnknown);

        // IMFAsyncCallback
        STDOVERRIDEMETHODIMP GetParameters(
            __RPC__out DWORD *pdwFlags,
            __RPC__out DWORD *pdwQueue);
        STDOVERRIDEMETHODIMP Invoke(
            __RPC__in_opt IMFAsyncResult *pAsyncResult);

    private:
        CriticalSection m_cs;
        boolean m_isShutdown;
        DWORD m_workItemQueueId;
        
        event<Windows::Foundation::EventHandler<Windows::Media::MediaProperties::MediaEncodingProfile>> m_profileEvent;
        event<Windows::Foundation::EventHandler<CameraCapture::Media::Payload>> m_payloadEvent;
        event<Windows::Foundation::EventHandler<Windows::Media::Core::MediaStreamSample>> m_streamSampleEvent;
        event<Windows::Foundation::EventHandler<Windows::Media::MediaProperties::MediaPropertySet>> m_metaDataEvent;
        event<Windows::Foundation::EventHandler<Windows::Media::MediaProperties::IMediaEncodingProperties>> m_mediaDescriptionEvent;

        CameraCapture::Media::Transform m_transform;
        Windows::Perception::Spatial::SpatialCoordinateSystem m_appCoordinateSystem;
    };
}

namespace winrt::CameraCapture::Media::factory_implementation
{
    struct PayloadHandler : PayloadHandlerT<PayloadHandler, implementation::PayloadHandler>
    {
    };
}
