﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Capture.MrcAudioEffect.h"
#include "Media.Capture.MrcAudioEffect.g.cpp"

using namespace winrt;
using namespace CameraCapture::Media::Capture::implementation;

MrcAudioEffect::MrcAudioEffect()
    : m_propertySet(Windows::Foundation::Collections::PropertySet{})
{
    MixerMode(DefaultAudioMixerMode);
}

hstring MrcAudioEffect::ActivatableClassId()
{
    return hstring(L"Windows.Media.MixedRealityCapture.MixedRealityCaptureAudioEffect");
}

Windows::Foundation::Collections::IPropertySet MrcAudioEffect::Properties()
{
    return m_propertySet;
}

CameraCapture::Media::Capture::AudioMixerMode MrcAudioEffect::MixerMode()
{
    AudioMixerMode result = DefaultAudioMixerMode;
    if (m_propertySet.HasKey(PROPERTY_MIXERMODE))
    {
        result = unbox_value<CameraCapture::Media::Capture::AudioMixerMode>((m_propertySet.Lookup(PROPERTY_MIXERMODE)));
    }

    return result;
}

void MrcAudioEffect::MixerMode(CameraCapture::Media::Capture::AudioMixerMode const& value)
{
    m_propertySet.Insert(PROPERTY_MIXERMODE, box_value<CameraCapture::Media::Capture::AudioMixerMode>(value));
}
