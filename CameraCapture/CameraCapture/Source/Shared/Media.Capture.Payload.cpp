// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Capture.Payload.h"
#include "Media.Capture.Payload.g.cpp"
#include "Media.Functions.h"

using namespace winrt;
using namespace CameraCapture::Media::Capture::implementation;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Media::Core;
using namespace Windows::Media::MediaProperties;


Payload::Payload(Windows::Media::MediaProperties::IMediaEncodingProperties const& encodingProperties)
	: m_mediaType(nullptr)
	, m_mediaSample(nullptr)
	, m_payloadType(PayloadType::Unknown)
	, m_encodingProperties(encodingProperties)
{
	if (_wcsicmp(m_encodingProperties.Type().c_str(), L"Audio") == 0)
	{
		m_payloadType = PayloadType::Audio;
	}
	else if (_wcsicmp(m_encodingProperties.Type().c_str(), L"Video") == 0)
	{
		m_payloadType = PayloadType::Video;
	}
	else if (_wcsicmp(m_encodingProperties.Type().c_str(), L"Container") == 0)
	{
		m_payloadType = PayloadType::Data;
	}
}

CameraCapture::Media::Capture::PayloadType Payload::Type()
{
	return m_payloadType;
}

IMediaEncodingProperties Payload::EncodingProperties()
{
	return m_encodingProperties;
}

Windows::Media::Core::MediaStreamSample Payload::MediaStreamSample()
{
	com_ptr<IMFMediaBuffer> spMediaBuffer;
	IFT(m_mediaSample->ConvertToContiguousBuffer(spMediaBuffer.put()));

	Windows::Storage::Streams::IBuffer sampleBuffer = nullptr;

	DWORD dwLength;
	BYTE *pbBuffer = nullptr;
	if (SUCCEEDED(spMediaBuffer->Lock(&pbBuffer, nullptr, &dwLength)))
	{
		sampleBuffer = make<CustomBuffer>(dwLength);
		auto spBufferByteAccess = sampleBuffer.as<IBufferByteAccess>();

		BYTE *pbRawIBuffer;
		if SUCCEEDED(spBufferByteAccess->Buffer(&pbRawIBuffer))
		{
			CopyMemory(pbRawIBuffer, pbBuffer, dwLength);

			sampleBuffer.Length(dwLength);
		}

		IFT(spMediaBuffer->Unlock());
	}

	LONGLONG llSampleTime;
	IFT(m_mediaSample->GetSampleTime(&llSampleTime));

	LONGLONG llSampleDuration = 0;
	IFT(m_mediaSample->GetSampleDuration(&llSampleDuration));

	auto sample = MediaStreamSample::CreateFromBuffer(sampleBuffer, Windows::Foundation::TimeSpan(llSampleTime));
	IFT(sample == nullptr ? E_OUTOFMEMORY: S_OK);

	IFT(CopyAttributesToMediaStreamSample(m_mediaSample, sample));

	if (llSampleDuration != 0)
	{
		sample.Duration(Windows::Foundation::TimeSpan(llSampleDuration));
	}

	return sample;
}

_Use_decl_annotations_
com_ptr<IMFSample> Payload::Sample()
{ 
	 return m_mediaSample; 
}

_Use_decl_annotations_
hresult Payload::Sample(IMFSample* pSample)
{
	m_mediaSample.copy_from(pSample);

	return S_OK;
}
