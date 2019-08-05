// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Capture.VideoPayload.h"
#include "Media.Capture.VideoPayload.g.cpp"

using namespace winrt;
using namespace CameraCapture::Media::Capture::implementation;
using namespace Windows::Media::MediaProperties;

using winrtPayload = CameraCapture::Media::Capture::Payload;

_Use_decl_annotations_
HRESULT VideoPayload::Create(
    IMFMediaType* pType,
    IMFSample* pSample,
    winrtPayload& payload)
{
    NULL_CHK_HR(pType, E_INVALIDARG);
    NULL_CHK_HR(pSample, E_INVALIDARG);

    payload = nullptr;

    auto videoPayload = make<VideoPayload>();
    IFR(videoPayload.as<IPayloadPriv>()->Initialize(pType, pSample));

    payload = videoPayload.as<winrtPayload>();

    return S_OK;
}

// IPayloadPriv
_Use_decl_annotations_
HRESULT VideoPayload::Initialize(
    IMFMediaType* pType,
    IMFSample* pSample)
{
    IFR(Payload::Initialize(pType, pSample));

    m_videoProperties = MediaEncodingProperties().as<VideoEncodingProperties>();

    return S_OK;
}

VideoEncodingProperties VideoPayload::VideoProperties()
{
    return m_videoProperties;
}
