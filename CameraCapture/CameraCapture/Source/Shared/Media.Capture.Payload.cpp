// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Capture.Payload.h"
#include "Media.Capture.Payload.g.cpp"

using namespace winrt;
using namespace CameraCapture::Media::Capture::implementation;
using namespace Windows::Media::MediaProperties;


// IPayloadPriv
_Use_decl_annotations_
HRESULT Payload::Initialize(
    IMFMediaType* pType, 
    IMFSample* pSample)
{
    NULL_CHK_HR(pType, E_INVALIDARG);
    NULL_CHK_HR(pSample, E_INVALIDARG);

    IFR(pType->QueryInterface(__uuidof(IMFMediaType), m_mediaType.put_void()));
    IFR(pSample->QueryInterface(__uuidof(IMFSample), m_mediaSample.put_void()));

    IFR(MFCreatePropertiesFromMediaType(m_mediaType.get(), guid_of<IMediaEncodingProperties>(), reinterpret_cast<void**>(put_abi(m_encodingProperties))));

    return S_OK;
}

_Use_decl_annotations_
IMFSample* Payload::Sample()
{ 
    return m_mediaSample.get(); 
}

_Use_decl_annotations_
HRESULT Payload::Sample(
    IMFSample* pSample)
{
    NULL_CHK_HR(pSample, E_INVALIDARG);

    IFR(pSample->QueryInterface(__uuidof(IMFSample), m_mediaSample.put_void()));

    return S_OK;
}

_Use_decl_annotations_
IMediaEncodingProperties Payload::MediaEncodingProperties()
{ 
    return m_encodingProperties; 
}
