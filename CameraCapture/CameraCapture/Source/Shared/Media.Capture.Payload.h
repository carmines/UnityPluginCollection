// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Media.Capture.Payload.g.h"

#include <mfidl.h>
#include <winrt/windows.media.mediaproperties.h>

struct __declspec(uuid("8300b3cc-c919-4c54-b01a-b375b843d3f8")) IStreamSample : ::IUnknown
{
    virtual winrt::com_ptr<IMFSample> __stdcall Sample() = 0;
    virtual winrt::hresult __stdcall Sample(_In_ IMFSample* pSample) = 0;
};


namespace winrt::CameraCapture::Media::Capture::implementation
{
    struct Payload : PayloadT<Payload, IStreamSample>
    {
		Payload() = default;

		Payload(Windows::Media::MediaProperties::IMediaEncodingProperties const& encodingProperties);
		CameraCapture::Media::Capture::PayloadType Type();
		Windows::Media::MediaProperties::IMediaEncodingProperties EncodingProperties();
		Windows::Media::Core::MediaStreamSample MediaStreamSample();

        // IStreamSample
        virtual com_ptr<IMFSample> __stdcall Sample() override;
        virtual winrt::hresult  __stdcall Sample(_In_ IMFSample* pSample) override;

    private:
		CameraCapture::Media::Capture::PayloadType m_payloadType;
		Windows::Media::MediaProperties::IMediaEncodingProperties m_encodingProperties;
        com_ptr<IMFMediaType> m_mediaType;
        com_ptr<IMFSample> m_mediaSample;
    };
}

namespace winrt::CameraCapture::Media::Capture::factory_implementation
{
	struct Payload : PayloadT<Payload, implementation::Payload>
	{
	};
}
