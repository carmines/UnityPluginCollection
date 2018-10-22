// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Capture.PayloadHandler.h"

using namespace winrt;
using namespace CameraCapture::Media::Capture::implementation;

using winrtPayload = CameraCapture::Media::Capture::Payload;

PayloadHandler::PayloadHandler()
    : m_isShutdown(false)
    , m_workItemQueueId(MFASYNC_CALLBACK_QUEUE_UNDEFINED)
{
    IFT(MFAllocateSerialWorkQueue(MFASYNC_CALLBACK_QUEUE_MULTITHREADED, &m_workItemQueueId));
}

_Use_decl_annotations_
HRESULT PayloadHandler::QueuePayload(
    winrtPayload const& payload)
{
    NULL_CHK_HR(payload, S_OK);

    slim_shared_lock_guard const guard(m_mutex);

    if (m_isShutdown)
    {
        IFR(MF_E_SHUTDOWN);
    }

    if (m_workItemQueueId == MFASYNC_CALLBACK_QUEUE_UNDEFINED)
    {
        return S_OK;
    }

    com_ptr<IMFAsyncResult> asyncResult = nullptr;
    IFR(MFCreateAsyncResult(nullptr, this, winrt::get_unknown(payload), asyncResult.put()));

    return MFPutWorkItemEx2(m_workItemQueueId, 0, asyncResult.detach());
}

_Use_decl_annotations_
HRESULT PayloadHandler::GetParameters(
    DWORD *pdwFlags,
    DWORD *pdwQueue)
{
    slim_shared_lock_guard const guard(m_mutex);

    if (m_isShutdown)
    {
        IFR(MF_E_SHUTDOWN);
    }

    *pdwFlags = 0;
    *pdwQueue = m_workItemQueueId;

    return S_OK;
}

_Use_decl_annotations_
HRESULT PayloadHandler::Invoke(
    IMFAsyncResult *pAsyncResult)
{
    // convert back to Payload
    com_ptr<IUnknown> spState = nullptr;
    spState.attach(pAsyncResult->GetStateNoAddRef());

    NULL_CHK_HR(spState, E_POINTER);

    auto payload = spState.as<winrtPayload>();

    HRESULT hr = S_OK;
    
    if (m_sampleEvent)
    {
        m_sampleEvent(*this, payload);
    }

    return pAsyncResult->SetStatus(hr);
}
