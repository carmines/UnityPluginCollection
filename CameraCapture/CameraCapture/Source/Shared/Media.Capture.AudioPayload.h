// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Media/Capture/AudioPayload.g.h"
#include "Media.Capture.Payload.h"

namespace winrt::CameraCapture::Media::Capture::implementation
{
    struct AudioPayload : AudioPayloadT<AudioPayload, implementation::Payload>
    {
        static HRESULT Create(
            _In_ IMFMediaType* pType, 
            _In_ IMFSample* pSample, 
            _Out_ Capture::Payload& payload);

        AudioPayload() 
            : m_audioProperties(nullptr)
        {
        }

        // IPayloadPriv
        STDOVERRIDEMETHODIMP Initialize(
            _In_ IMFMediaType* pType,
            _In_ IMFSample* pSample);

        Windows::Media::MediaProperties::AudioEncodingProperties AudioProperties();

    private:
        Windows::Media::MediaProperties::AudioEncodingProperties m_audioProperties;
    };
}

namespace winrt::CameraCapture::Media::Capture::factory_implementation
{
    struct AudioPayload : AudioPayloadT<AudioPayload, implementation::AudioPayload>
    {
    };
}
