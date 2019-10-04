// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.PayloadHandler.h"
#include "Media.PayloadHandler.g.cpp"
#include "Media.Payload.h"

#include <winrt/windows.media.h>
#include <winrt/windows.media.core.h>

using namespace winrt;
using namespace CameraCapture::Media::implementation;
using namespace Windows::Media::Core;
using namespace Windows::Media::MediaProperties;

PayloadHandler::PayloadHandler()
    : m_isShutdown(false)
    , m_workItemQueueId(MFASYNC_CALLBACK_QUEUE_UNDEFINED)
{
    IFT(MFStartup(MF_VERSION));

    IFT(MFAllocateSerialWorkQueue(MFASYNC_CALLBACK_QUEUE_MULTITHREADED, &m_workItemQueueId));
}

void PayloadHandler::Close()
{
    auto gurad = m_cs.Guard();

    m_isShutdown = true;

    MFShutdown();
}

void PayloadHandler::QueueEncodingProfile(MediaEncodingProfile const& mediaProfile)
{
    QueueUnknown(winrt::get_unknown(mediaProfile));
}

void PayloadHandler::QueueMetadata(MediaPropertySet const& metaData)
{
    QueueUnknown(winrt::get_unknown(metaData));
}

void PayloadHandler::QueueEncodingProperties(Windows::Media::MediaProperties::IMediaEncodingProperties const& mediaDescription)
{
    QueueUnknown(winrt::get_unknown(mediaDescription));
}

void PayloadHandler::QueuePayload(CameraCapture::Media::Payload const& payload)
{
    QueueUnknown(winrt::get_unknown(payload));
}

_Use_decl_annotations_
HRESULT PayloadHandler::QueueMFSample(
    GUID majorType, 
    com_ptr<IMFMediaType> const& type, 
    com_ptr<IMFSample> const& sample)
{
    auto payload = make<Media::implementation::Payload>();

    payload.as<IStreamSample>()->Sample(majorType, type, sample);
    
    return QueueUnknown(winrt::get_unknown(payload));
}

_Use_decl_annotations_
HRESULT PayloadHandler::QueueUnknown(
    IUnknown* pUnknown)
{
    NULL_CHK_HR(pUnknown, S_OK);

    auto gurad = m_cs.Guard();

    if (m_isShutdown)
    {
        IFR(MF_E_SHUTDOWN);
    }

    if (m_workItemQueueId == MFASYNC_CALLBACK_QUEUE_UNDEFINED)
    {
        return S_OK;
    }

    com_ptr<IMFAsyncResult> asyncResult = nullptr;
    IFR(MFCreateAsyncResult(nullptr, this, pUnknown, asyncResult.put()));

    return MFPutWorkItemEx2(m_workItemQueueId, 0, asyncResult.detach());
}

_Use_decl_annotations_
HRESULT PayloadHandler::GetParameters(
    DWORD *pdwFlags,
    DWORD *pdwQueue)
{
    auto gurad = m_cs.Guard();

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
    if (m_isShutdown)
    {
        return S_OK;
    }

    // convert back to Payload
    com_ptr<IUnknown> spState = nullptr;
    spState.attach(pAsyncResult->GetStateNoAddRef());

    NULL_CHK_HR(spState, E_POINTER);
        
    hresult hr = S_OK;

    auto payload = spState.try_as<CameraCapture::Media::Payload>();
    auto profile = spState.try_as<MediaEncodingProfile>();
    auto metaData = spState.try_as<MediaPropertySet>();
    auto mediaDescription = spState.try_as<IMediaEncodingProperties>();
    auto streamSample = spState.try_as<MediaStreamSample>();
    if (profile != nullptr)
    {
        if (m_profileEvent)
        {
            m_profileEvent(*this, profile);
        }
    }
    else if (payload != nullptr)
    {
        if (m_payloadEvent)
        {
            m_payloadEvent(*this, payload);
        }
    } 
    else if (metaData != nullptr)
    {
        if (m_metaDataEvent)
        {
            m_metaDataEvent(*this, metaData);
        }
    } 
    else if (mediaDescription != nullptr)
    {
        if (m_mediaDescriptionEvent)
        {
            m_mediaDescriptionEvent(*this, mediaDescription);
        }
    }
    else if (streamSample != nullptr)
    {
        if (m_streamSampleEvent)
        {
            m_streamSampleEvent(*this, streamSample);
        }
    }

    return pAsyncResult->SetStatus(hr);
}
