// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Media/Capture/PayloadHandler.g.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>

namespace winrt::CameraCapture::Media::Capture::implementation
{
    struct PayloadHandler : PayloadHandlerT<PayloadHandler, IMFAsyncCallback>
    {
        PayloadHandler();

        // IMFAsyncCallback
        STDOVERRIDEMETHODIMP GetParameters(
            __RPC__out DWORD *pdwFlags,
            __RPC__out DWORD *pdwQueue);
        STDOVERRIDEMETHODIMP Invoke(
            __RPC__in_opt IMFAsyncResult *pAsyncResult);

        // PayloadHandler
        HRESULT QueuePayload(Capture::Payload const& payload);

        event_token OnSample(Windows::Foundation::EventHandler<Capture::Payload> const& handler)
        {
            return m_sampleEvent.add(handler);
        }
        void OnSample(event_token const& token)
        {
            m_sampleEvent.remove(token);
        }

    private:
        slim_mutex m_mutex;
        boolean m_isShutdown;
        DWORD m_workItemQueueId;

        event<Windows::Foundation::EventHandler<Capture::Payload>> m_sampleEvent;
    };
}

namespace winrt::CameraCapture::Media::Capture::factory_implementation
{
    struct PayloadHandler : PayloadHandlerT<PayloadHandler, implementation::PayloadHandler>
    {
    };
}
