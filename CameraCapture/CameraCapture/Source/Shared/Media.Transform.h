#pragma once
#include "Media.Transform.g.h"

#include <winrt/windows.perception.spatial.h>
#include <mfapi.h>

struct __declspec(uuid("27ee71f8-e7d3-435c-b394-42058efa6591")) ITransformPriv : ::IUnknown
{
    virtual winrt::hresult GetTransforms(
        _In_ winrt::com_ptr<IMFSample> const& sourceSample, 
        _In_ winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem,
        _Out_ winrt::Windows::Foundation::Numerics::float4x4& transform,
        _Out_ winrt::Windows::Foundation::Numerics::float4x4& projection) = 0;
};

namespace winrt::CameraCapture::Media::implementation
{
    struct Transform : TransformT<Transform, ITransformPriv>
    {
        Transform();
        ~Transform() { Reset(); }

        // Trasform
        bool ProcessWorldTransform(
            Media::Payload const& payload, 
            Windows::Perception::Spatial::SpatialCoordinateSystem const& worldOrigin);

        // ITransformPriv
        virtual hresult GetTransforms(
            _In_ com_ptr<IMFSample> const& sourceSample,
            _In_ Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem,
            _Out_ Windows::Foundation::Numerics::float4x4& transform,
            _Out_ Windows::Foundation::Numerics::float4x4& projection) override;

    private:
        void Reset();

        hresult Update(
            _In_ Media::Payload const& payload,
            _In_ Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem);

        hresult UpdateV2(
            _In_ Media::Payload const& payload,
            _In_ Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem);

    private:
        boolean m_useNewApi;
        guid m_currentDynamicNodeId;
        Windows::Perception::Spatial::SpatialLocator m_locator{ nullptr };
        Windows::Perception::Spatial::SpatialLocatorAttachedFrameOfReference m_frameOfReference{ nullptr };
    };
}

namespace winrt::CameraCapture::Media::factory_implementation
{
    struct Transform : TransformT<Transform, implementation::Transform>
    {
    };
}
