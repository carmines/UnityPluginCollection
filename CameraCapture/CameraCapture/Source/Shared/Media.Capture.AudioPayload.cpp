#include "pch.h"
#include "Media.Capture.AudioPayload.h"

using namespace winrt;
using namespace CameraCapture::Media::Capture::implementation;
using namespace Windows::Media::MediaProperties;

using winrtPayload = CameraCapture::Media::Capture::Payload;

_Use_decl_annotations_
HRESULT AudioPayload::Create(
    IMFMediaType* pType, 
    IMFSample* pSample, 
    winrtPayload& payload)
{
    NULL_CHK_HR(pType, E_INVALIDARG);
    NULL_CHK_HR(pSample, E_INVALIDARG);

    payload = nullptr;

    auto audioPayload = make<AudioPayload>();
    IFR(audioPayload.as<IPayloadPriv>()->Initialize(pType, pSample));

    payload = audioPayload.as<winrtPayload>();

    return S_OK;
}

// IPayloadPriv
_Use_decl_annotations_
HRESULT AudioPayload::Initialize(
    IMFMediaType* pType,
    IMFSample* pSample)
{
    IFR(Payload::Initialize(pType, pSample));

    m_audioProperties = MediaEncodingProperties().as<AudioEncodingProperties>();

    return S_OK;
}

AudioEncodingProperties AudioPayload::AudioProperties()
{
    return m_audioProperties;
}

