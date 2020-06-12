// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Media.Payload.g.h"

#include <mfidl.h>
#include <winrt/windows.media.mediaproperties.h>

// {09F82036-0542-4561-84A1-C59FEDA47403}
extern const __declspec(selectany) winrt::guid MF_PAYLOAD_FLUSH =
{ 0x9f82036, 0x542, 0x4561, { 0x84, 0xa1, 0xc5, 0x9f, 0xed, 0xa4, 0x74, 0x3 } };
// {ECE89B13-173B-499D-B30F-9E30ACEE2265}
extern const __declspec(selectany) winrt::guid MF_PAYLOAD_MARKER_TYPE =
    { 0xece89b13, 0x173b, 0x499d, { 0xb3, 0xf, 0x9e, 0x30, 0xac, 0xee, 0x22, 0x65 } };
// {13B9D157-27E7-468F-B5F6-664E12A8FF43}
extern const __declspec(selectany) winrt::guid MF_PAYLOAD_MARKER_TICK =
    { 0x13b9d157, 0x27e7, 0x468f, { 0xb5, 0xf6, 0x66, 0x4e, 0x12, 0xa8, 0xff, 0x43 } };
// {86E63DA3-A537-4887-AE1A-18BBE99FCE09}
extern const __declspec(selectany) winrt::guid MF_PAYLOAD_MARKER_TICK_TIMESTAMP =
{ 0x86e63da3, 0xa537, 0x4887, { 0xae, 0x1a, 0x18, 0xbb, 0xe9, 0x9f, 0xce, 0x9 } };

struct __declspec(uuid("8300b3cc-c919-4c54-b01a-b375b843d3f8")) IStreamSample : ::IUnknown
{
    virtual winrt::com_ptr<IMFSample> __stdcall Sample() = 0;
    virtual winrt::hresult __stdcall Sample(
        _In_ winrt::guid const& majorType,
        _In_ winrt::com_ptr<IMFMediaType> const& mediaType, 
        _In_ winrt::com_ptr<IMFSample> const& sample) = 0;
    virtual void __stdcall SetTransformAndProjection(
        _In_ winrt::Windows::Foundation::Numerics::float4x4 const& cameraTranform,
        _In_ winrt::Windows::Foundation::Numerics::float4x4 const& cameraProjection) = 0;
};

namespace winrt::CameraCapture::Media::implementation
{
    struct Payload : PayloadT<Payload, IStreamSample>
    {
        Payload();

        Windows::Media::MediaProperties::MediaPropertySet MediaPropertySet();
        Windows::Media::MediaProperties::IMediaEncodingProperties EncodingProperties();
        Windows::Media::Core::MediaStreamSample MediaStreamSample();

        bool HasTransform() { return m_hasTransform; }
        Windows::Foundation::Numerics::float4x4 CameraToWorld() { return m_cameraToWorld; }
        Windows::Foundation::Numerics::float4x4 CameraProjection() { return m_cameraProjection; }

        // IStreamSample
        virtual winrt::com_ptr<IMFSample> __stdcall Sample() override;
        virtual hresult __stdcall Sample(
            _In_ guid const& majorType,
            _In_ com_ptr<IMFMediaType> const& mediaType, 
            _In_ com_ptr<IMFSample> const& sample) override;
        virtual void __stdcall SetTransformAndProjection(
            _In_ Windows::Foundation::Numerics::float4x4 const& cameraTranform,
            _In_ Windows::Foundation::Numerics::float4x4 const& cameraProjection) override;

    private:
        com_ptr<IMFMediaType> m_mediaType;
        com_ptr<IMFSample> m_mediaSample;

        Windows::Media::MediaProperties::MediaPropertySet m_propertySet;
        Windows::Media::MediaProperties::IMediaEncodingProperties m_encodingProperties;
        Windows::Media::Core::MediaStreamSample m_mediaStreamSample;

        bool m_hasTransform;
        Windows::Foundation::Numerics::float4x4 m_cameraToWorld;
        Windows::Foundation::Numerics::float4x4 m_cameraProjection;
    };
}

namespace winrt::CameraCapture::Media::factory_implementation
{
    struct Payload : PayloadT<Payload, implementation::Payload>
    {
    };
}
