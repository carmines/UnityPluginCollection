#pragma once
#include "Media.Transform.g.h"

#include <winrt/windows.perception.spatial.h>
#include <mfapi.h>


struct __declspec(uuid("27ee71f8-e7d3-435c-b394-42058efa6591")) ITransformPriv : ::IUnknown
{
    virtual winrt::hresult __stdcall Initialize(_In_ winrt::com_ptr<IMFSample> const& sourceSample) = 0;
    virtual winrt::hresult __stdcall Update(
        _In_ winrt::com_ptr<IMFSample> const& sourceSample, 
        _In_ winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem) = 0;
};

namespace winrt::CameraCapture::Media::implementation
{
    struct Transform : TransformT<Transform, ITransformPriv>
    {
        static Media::Transform Create(
            _In_ com_ptr<IMFSample> sourceSample);

        Transform();
        ~Transform() { Reset(); }

        // ITranform
        virtual hresult __stdcall Initialize(_In_ com_ptr<IMFSample> const& sourceSample) override;
        virtual hresult __stdcall Update(
            _In_ com_ptr<IMFSample> const& sourceSample,
            _In_ Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem) override;

        // Transform
        void Reset();

        hresult Update(Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem);

        Windows::Foundation::Numerics::float4x4 CameraToWorldTransform();
        Windows::Foundation::Numerics::float4x4 CameraProjectionMatrix();

    private:
        hresult UpdateV2(
            _In_ Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem);

        // Convert a duration value from a source tick frequency to a destination tick frequency.
        static inline int64_t SourceDurationTicksToDestDurationTicks(int64_t sourceDurationInTicks, int64_t sourceTicksPerSecond, int64_t destTicksPerSecond)
        {
            int64_t whole = (sourceDurationInTicks / sourceTicksPerSecond) * destTicksPerSecond;                          // 'whole' is rounded down in the target time units.
            int64_t part = (sourceDurationInTicks % sourceTicksPerSecond) * destTicksPerSecond / sourceTicksPerSecond;    // 'part' is the remainder in the target time units.
            return whole + part;
        }

        static inline winrt::Windows::Foundation::TimeSpan TimeSpanFromQpcTicks(int64_t qpcTicks)
        {
            static const int64_t qpcFrequency = []
            {
                LARGE_INTEGER frequency;
                QueryPerformanceFrequency(&frequency);
                return frequency.QuadPart;
            }();

            return winrt::Windows::Foundation::TimeSpan{ SourceDurationTicksToDestDurationTicks(qpcTicks, qpcFrequency, winrt::clock::period::den) / winrt::clock::period::num };
        }

        static inline winrt::Windows::Foundation::Numerics::float4x4 GetProjection(MFPinholeCameraIntrinsics const& cameraIntrinsics)
        {
            // Default camera projection, which has
            // scale up 2.0f to x and y for (-1, -1) to (1, 1) viewport 
            // and taking camera affine
            winrt::Windows::Foundation::Numerics::float4x4 const defaultProjection(
                2.0f, 0.0f, 0.0f, 0.0f,
                0.0f, -2.0f, 0.0f, 0.0f,
                -1.0f, 1.0f, 1.0f, 1.0f,
                0.0f, 0.0f, 0.0f, 0.0f);

            float fx = cameraIntrinsics.IntrinsicModels[0].CameraModel.FocalLength.x / static_cast<float>(cameraIntrinsics.IntrinsicModels[0].Width);
            float fy = cameraIntrinsics.IntrinsicModels[0].CameraModel.FocalLength.y / static_cast<float>(cameraIntrinsics.IntrinsicModels[0].Height);
            float px = cameraIntrinsics.IntrinsicModels[0].CameraModel.PrincipalPoint.x / static_cast<float>(cameraIntrinsics.IntrinsicModels[0].Width);
            float py = cameraIntrinsics.IntrinsicModels[0].CameraModel.PrincipalPoint.y / static_cast<float>(cameraIntrinsics.IntrinsicModels[0].Height);

            winrt::Windows::Foundation::Numerics::float4x4 const cameraAffine(
                fx, 0.0f, 0.0f, 0.0f,
                0.0f, -fy, 0.0f, 0.0f,
                -px, -py, -1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f);

            return cameraAffine * defaultProjection;
        }

    private:
        boolean m_useNewApi;
        guid m_currentDynamicNodeId;
        Windows::Perception::Spatial::SpatialLocator m_locator{ nullptr };
        Windows::Perception::Spatial::SpatialLocatorAttachedFrameOfReference m_frameOfReference{ nullptr };
        
        Windows::Foundation::Numerics::float4x4 m_cameraToWorldTransform;
        Windows::Foundation::Numerics::float4x4 m_cameraProjectionMatrix;

        com_ptr<IMFSample> m_sample;
    };
}

namespace winrt::CameraCapture::Media::factory_implementation
{
    struct Transform : TransformT<Transform, implementation::Transform>
    {
    };
}
