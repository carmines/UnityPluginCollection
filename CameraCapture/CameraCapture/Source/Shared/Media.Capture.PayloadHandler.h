// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Media.Capture.PayloadHandler.g.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>

namespace winrt::CameraCapture::Media::Capture::implementation
{
    struct PayloadHandler : PayloadHandlerT<PayloadHandler, IMFAsyncCallback>
    {
		PayloadHandler();

        // PayloadHandler
		winrt::hresult QueuePayload(CameraCapture::Media::Capture::Payload const& payload);
		winrt::event_token OnSample(Windows::Foundation::EventHandler<CameraCapture::Media::Capture::Payload> const& handler)
		{
			std::lock_guard<slim_mutex> guard(m_mutex);

			return m_sampleEvent.add(handler);
		}
		void OnSample(winrt::event_token const& token) noexcept
		{
			std::lock_guard<slim_mutex> guard(m_mutex);

			m_sampleEvent.remove(token);
		}

        // IMFAsyncCallback
        STDOVERRIDEMETHODIMP GetParameters(
            __RPC__out DWORD *pdwFlags,
            __RPC__out DWORD *pdwQueue);
        STDOVERRIDEMETHODIMP Invoke(
            __RPC__in_opt IMFAsyncResult *pAsyncResult);

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
