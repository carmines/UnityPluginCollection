// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Media.Capture.MrcVideoEffect.g.h"

namespace winrt::CameraCapture::Media::Capture::implementation
{

#define PROPERTY_GLOBALOPACITYCOEFFICIENT L"GlobalOpacityCoefficient"
#define PROPERTY_HOLOGRAMCOMPOSITIONENABLED L"HologramCompositionEnabled"
#define PROPERTY_RECORDINGINDICATORENABLED L"RecordingIndicatorEnabled"
#define PROPERTY_STREAMTYPE L"StreamType"
#define PROPERTY_VIDEOSTABILIZATIONENABLED L"VideoStabilizationEnabled"
#define PROPERTY_VIDEOSTABILIZATIONBUFFERLENGTH L"VideoStabilizationBufferLength"
#define PROPERTY_MAX_VSBUFFER 30U

    struct MrcVideoEffect : MrcVideoEffectT<MrcVideoEffect>
    {
        MrcVideoEffect();

        // IVideoEffectDefinition
        hstring ActivatableClassId();
        Windows::Foundation::Collections::IPropertySet Properties();

        // MrcVideoEffect
        float GlobalOpacityCoefficient();
        void GlobalOpacityCoefficient(float value);

        bool HologramCompositionEnabled();
        void HologramCompositionEnabled(bool value);

        bool RecordingIndicatorEnabled();
        void RecordingIndicatorEnabled(bool value);

        Windows::Media::Capture::MediaStreamType StreamType();
        void StreamType(Windows::Media::Capture::MediaStreamType const& value);

        bool VideoStabilizationEnabled();
        void VideoStabilizationEnabled(bool value);

        uint32_t VideoStabilizationBufferLength();
        void VideoStabilizationBufferLength(uint32_t value);

        uint32_t VideoStabilizationMaximumBufferLength();

    private:
        Windows::Foundation::Collections::PropertySet m_propertySet;

        static constexpr float DefaultGlobalOpacityCoefficient = 0.9f;
        static constexpr bool DefaultHologramCompositionEnabled = true;
        static constexpr Windows::Media::Capture::MediaStreamType DefaultStreamType = Windows::Media::Capture::MediaStreamType::VideoRecord;
        static constexpr bool DefaultRecordingIndicatorEnabled = true;
        static constexpr bool DefaultVideoStabilizationEnabled = false;
        static constexpr uint32_t DefaultVideoStabilizationBufferLength = 0U;
    };
}

namespace winrt::CameraCapture::Media::Capture::factory_implementation
{
    struct MrcVideoEffect : MrcVideoEffectT<MrcVideoEffect, implementation::MrcVideoEffect>
    {
    };
}
