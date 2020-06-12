// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Payload.h"
#include "Media.Payload.g.cpp"
#include "Media.Functions.h"

#include <mfapi.h>

using namespace winrt;
using namespace CameraCapture::Media::implementation;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::DirectX::Direct3D11;
using namespace Windows::Media::Core;
using namespace Windows::Media::MediaProperties;

Payload::Payload()
    : m_mediaType(nullptr)
    , m_mediaSample(nullptr)
    , m_propertySet()
    , m_encodingProperties(nullptr)
    , m_mediaStreamSample(nullptr)
    , m_hasTransform(false)
    , m_cameraToWorld()
    , m_cameraProjection()
{
}

MediaPropertySet Payload::MediaPropertySet()
{
    return m_propertySet;
}

IMediaEncodingProperties Payload::EncodingProperties()
{
    if (m_encodingProperties == nullptr && m_mediaType != nullptr)
    {
        IFT(MFCreatePropertiesFromMediaType(m_mediaType.get(), guid_of<IMediaEncodingProperties>(), put_abi(m_encodingProperties)));
    }

    return m_encodingProperties;
}

MediaStreamSample Payload::MediaStreamSample()
{
    return m_mediaStreamSample;
}

_Use_decl_annotations_
com_ptr<IMFSample> Payload::Sample()
{ 
     return m_mediaSample; 
}

_Use_decl_annotations_
hresult Payload::Sample(
    winrt::guid const& majorType,
    com_ptr<IMFMediaType> const& mediaType, 
    com_ptr<IMFSample> const& mediaSample)
{
    m_hasTransform = false;

    // convert to mediastreamsample
    LONGLONG sampleTime = 0;
    IFR(mediaSample->GetSampleTime(&sampleTime));

    Windows::Media::Core::MediaStreamSample streamSample = nullptr;
    IFR(CreateMediaStreamSample(mediaSample, TimeSpan(sampleTime), streamSample));

    winrt::guid type = majorType;
    streamSample.ExtendedProperties().Insert(MF_MT_MAJOR_TYPE, box_value(type));

    // store objects
    m_mediaType = mediaType;
    m_mediaSample = mediaSample;
    m_mediaStreamSample = streamSample;

    return S_OK;
}

_Use_decl_annotations_
void Payload::SetTransformAndProjection(
    _In_ Windows::Foundation::Numerics::float4x4 const& cameraToWorld,
    _In_ Windows::Foundation::Numerics::float4x4 const& cameraProjection)
{
    m_hasTransform = true;
    m_cameraToWorld = cameraToWorld;
    m_cameraProjection = cameraProjection;
}
