// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Media.Capture.Payload.g.h"

#include <mfidl.h>
#include <winrt/windows.media.mediaproperties.h>

struct __declspec(uuid("38345d37-8f05-45f3-b712-0219cbd6b2b4")) IPayloadPriv : ::IUnknown
{
    STDMETHOD(Initialize)(_In_ IMFMediaType* pType, _In_ IMFSample* pSample) PURE;
};

struct __declspec(uuid("8300b3cc-c919-4c54-b01a-b375b843d3f8")) IStreamSample : ::IUnknown
{
    STDMETHOD_(IMFSample*, Sample)() PURE;
    STDMETHOD(Sample)(_In_ IMFSample* pSample) PURE;
};

namespace winrt::CameraCapture::Media::Capture::implementation
{
    struct Payload : PayloadT<Payload, IStreamSample, IPayloadPriv>
    {
        Payload()
            : m_encodingProperties(nullptr)
            , m_mediaType(nullptr)
            , m_mediaSample(nullptr)
        {
        }

        // IPayloadPriv
        STDOVERRIDEMETHODIMP Initialize(
            _In_ IMFMediaType* pType,
            _In_ IMFSample* pSample);

        // IStreamSample
        STDOVERRIDEMETHODIMP_(IMFSample*) Sample();
        STDOVERRIDEMETHODIMP Sample(_In_ IMFSample* pSample);

        Windows::Media::MediaProperties::IMediaEncodingProperties MediaEncodingProperties();

    private:
        com_ptr<IMFMediaType> m_mediaType;
        com_ptr<IMFSample> m_mediaSample;
        Windows::Media::MediaProperties::IMediaEncodingProperties m_encodingProperties;
    };
}
