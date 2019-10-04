// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Capture.MrcVideoEffect.h"
#include "Media.Capture.MrcVideoEffect.g.cpp"

using namespace winrt;
using namespace CameraCapture::Media::Capture::implementation;
using namespace Windows::Media::Capture;

MrcVideoEffect::MrcVideoEffect()
    : m_propertySet()
{
    // Set Default values
    StreamType(DefaultStreamType);
    HologramCompositionEnabled(DefaultHologramCompositionEnabled);
    RecordingIndicatorEnabled(DefaultRecordingIndicatorEnabled);
    VideoStabilizationEnabled(DefaultVideoStabilizationEnabled);
    VideoStabilizationBufferLength(15);
    GlobalOpacityCoefficient(DefaultGlobalOpacityCoefficient);
}

hstring MrcVideoEffect::ActivatableClassId()
{
    return hstring(L"Windows.Media.MixedRealityCapture.MixedRealityCaptureVideoEffect");
}

Windows::Foundation::Collections::IPropertySet MrcVideoEffect::Properties()
{
    return m_propertySet;
}

float MrcVideoEffect::GlobalOpacityCoefficient()
{
    float result = DefaultGlobalOpacityCoefficient;
    if (m_propertySet.HasKey(PROPERTY_GLOBALOPACITYCOEFFICIENT))
    {
        result = unbox_value<float>((m_propertySet.Lookup(PROPERTY_GLOBALOPACITYCOEFFICIENT)));
    }

    return result;
}
void MrcVideoEffect::GlobalOpacityCoefficient(float value)
{
    m_propertySet.Insert(PROPERTY_GLOBALOPACITYCOEFFICIENT, box_value<float>(value));
}

bool MrcVideoEffect::HologramCompositionEnabled()
{
    bool result = DefaultHologramCompositionEnabled;
    if (m_propertySet.HasKey(PROPERTY_HOLOGRAMCOMPOSITIONENABLED))
    {
        result = unbox_value<bool>((m_propertySet.Lookup(PROPERTY_HOLOGRAMCOMPOSITIONENABLED)));
    }

    return result;
}
void MrcVideoEffect::HologramCompositionEnabled(bool value)
{
    m_propertySet.Insert(PROPERTY_HOLOGRAMCOMPOSITIONENABLED, box_value<bool>(value));
}

bool MrcVideoEffect::RecordingIndicatorEnabled()
{
    bool result = DefaultRecordingIndicatorEnabled;
    if (m_propertySet.HasKey(PROPERTY_RECORDINGINDICATORENABLED))
    {
        result = unbox_value<bool>(m_propertySet.Lookup(PROPERTY_RECORDINGINDICATORENABLED));
    }

    return result;
}
void MrcVideoEffect::RecordingIndicatorEnabled(bool value)
{
    m_propertySet.Insert(PROPERTY_RECORDINGINDICATORENABLED, box_value<bool>(value));
}

MediaStreamType MrcVideoEffect::StreamType()
{
    MediaStreamType result = DefaultStreamType;
    if (m_propertySet.HasKey(PROPERTY_STREAMTYPE))
    {
        result = unbox_value<MediaStreamType>((m_propertySet.Lookup(PROPERTY_STREAMTYPE)));
    }

    return result;
}
void MrcVideoEffect::StreamType(MediaStreamType const& value)
{
    m_propertySet.Insert(PROPERTY_STREAMTYPE, box_value<MediaStreamType, uint32_t>(value));
}

bool MrcVideoEffect::VideoStabilizationEnabled()
{
    bool result = DefaultVideoStabilizationEnabled;
    if (m_propertySet.HasKey(PROPERTY_VIDEOSTABILIZATIONENABLED))
    {
        result = unbox_value<bool>(m_propertySet.Lookup(PROPERTY_VIDEOSTABILIZATIONENABLED));
    }

    return result;
}
void MrcVideoEffect::VideoStabilizationEnabled(bool value)
{
    m_propertySet.Insert(PROPERTY_VIDEOSTABILIZATIONENABLED, box_value<bool>(value));
}

uint32_t MrcVideoEffect::VideoStabilizationBufferLength()
{
    uint32_t result = DefaultVideoStabilizationBufferLength;
    if (m_propertySet.HasKey(PROPERTY_VIDEOSTABILIZATIONBUFFERLENGTH))
    {
        result = unbox_value<uint32_t>(m_propertySet.Lookup(PROPERTY_VIDEOSTABILIZATIONBUFFERLENGTH));
    }

    return result;
}
void MrcVideoEffect::VideoStabilizationBufferLength(uint32_t value)
{
    m_propertySet.Insert(PROPERTY_VIDEOSTABILIZATIONBUFFERLENGTH, box_value<uint32_t>(value));
}

uint32_t MrcVideoEffect::VideoStabilizationMaximumBufferLength()
{
    return PROPERTY_MAX_VSBUFFER;
}
