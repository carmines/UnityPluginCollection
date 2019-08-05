// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Media.Capture.MrcAudioEffect.g.h"
#include <winrt/Windows.Foundation.Collections.h>

namespace winrt::CameraCapture::Media::Capture::implementation
{
#define PROPERTY_MIXERMODE L"MixerMode"

    struct MrcAudioEffect : MrcAudioEffectT<MrcAudioEffect>
    {
        MrcAudioEffect();

        // IVideoEffectDefinition
        hstring ActivatableClassId();
        Windows::Foundation::Collections::IPropertySet Properties();

        // MrcAudioEffect
        Capture::AudioMixerMode MixerMode();
        void MixerMode(Capture::AudioMixerMode const& value);

    private:
        static constexpr Capture::AudioMixerMode DefaultAudioMixerMode = Capture::AudioMixerMode::MicAndLoopback;

        Windows::Foundation::Collections::PropertySet m_propertySet;
    };
}

namespace winrt::CameraCapture::Media::Capture::factory_implementation
{
    struct MrcAudioEffect : MrcAudioEffectT<MrcAudioEffect, implementation::MrcAudioEffect>
    {
    };
}
