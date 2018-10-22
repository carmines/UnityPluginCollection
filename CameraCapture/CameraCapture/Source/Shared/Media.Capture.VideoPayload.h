// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Media/Capture/VideoPayload.g.h"
#include "Media.Capture.Payload.h"

namespace winrt::CameraCapture::Media::Capture::implementation
{
    struct VideoPayload : VideoPayloadT<VideoPayload, Capture::implementation::Payload>
    {
        static HRESULT Create(
            _In_ IMFMediaType* pType,
            _In_ IMFSample* pSample,
            _Out_ Capture::Payload& payload);

        VideoPayload() 
            : m_videoProperties(nullptr)
        {
        }

        // IPayloadPriv
        STDOVERRIDEMETHODIMP Initialize(
            _In_ IMFMediaType* pType,
            _In_ IMFSample* pSample);

        Windows::Media::MediaProperties::VideoEncodingProperties VideoProperties();

    private:
        Windows::Media::MediaProperties::VideoEncodingProperties m_videoProperties;
    };
}

namespace winrt::CameraCapture::Media::Capture::factory_implementation
{
    struct VideoPayload : VideoPayloadT<VideoPayload, implementation::VideoPayload>
    {
    };
}
