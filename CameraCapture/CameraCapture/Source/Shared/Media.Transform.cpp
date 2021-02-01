#include "pch.h"
#include "Media.Transform.h"
#include "Media.Transform.g.cpp"

#include <Media.Payload.h>

#include <winrt/windows.perception.spatial.preview.h>
#include <winrt/windows.foundation.metadata.h>

#include <mfidl.h>
#include <mferror.h>

using namespace winrt;
using namespace CameraCapture::Media::implementation;

using namespace Windows::Foundation;
using namespace Windows::Foundation::Metadata;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Perception;
using namespace Windows::Perception::Spatial;
using namespace Windows::Perception::Spatial::Preview;

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
EXTERN_GUID(MFSampleExtension_DeviceTimestamp, 0x8f3e35e7, 0x2dcd, 0x4887, 0x86, 0x22, 0x2a, 0x58, 0xba, 0xa6, 0x52, 0xb0);
EXTERN_GUID(MFSampleExtension_Spatial_CameraCoordinateSystem, 0x9d13c82f, 0x2199, 0x4e67, 0x91, 0xcd, 0xd1, 0xa4, 0x18, 0x1f, 0x25, 0x34);
EXTERN_GUID(MFSampleExtension_Spatial_CameraViewTransform, 0x4e251fa4, 0x830f, 0x4770, 0x85, 0x9a, 0x4b, 0x8d, 0x99, 0xaa, 0x80, 0x9b);
EXTERN_GUID(MFSampleExtension_Spatial_CameraProjectionTransform, 0x47f9fcb5, 0x2a02, 0x4f26, 0xa4, 0x77, 0x79, 0x2f, 0xdf, 0x95, 0x88, 0x6a);
#endif

static inline Windows::Foundation::Numerics::float4x4 GetProjection(MFPinholeCameraIntrinsics const& cameraIntrinsics)
{
    // Default camera projection, which has
    // scale up 2.0f to x and y for (-1, -1) to (1, 1) viewport 
    // and taking camera affine
    Windows::Foundation::Numerics::float4x4 const defaultProjection(
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f, 0.0f, 0.0f,
        -1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 0.0f);

    float fx = cameraIntrinsics.IntrinsicModels[0].CameraModel.FocalLength.x / static_cast<float>(cameraIntrinsics.IntrinsicModels[0].Width);
    float fy = cameraIntrinsics.IntrinsicModels[0].CameraModel.FocalLength.y / static_cast<float>(cameraIntrinsics.IntrinsicModels[0].Height);
    float px = cameraIntrinsics.IntrinsicModels[0].CameraModel.PrincipalPoint.x / static_cast<float>(cameraIntrinsics.IntrinsicModels[0].Width);
    float py = cameraIntrinsics.IntrinsicModels[0].CameraModel.PrincipalPoint.y / static_cast<float>(cameraIntrinsics.IntrinsicModels[0].Height);

    Windows::Foundation::Numerics::float4x4 const cameraAffine(
        fx, 0.0f, 0.0f, 0.0f,
        0.0f, -fy, 0.0f, 0.0f,
        -px, -py, -1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    return cameraAffine * defaultProjection;
}

Transform::Transform()
    : m_useNewApi(
        ApiInformation::IsApiContractPresent(L"Windows.Foundation.UniversalApiContract", 8)
        &&
        ApiInformation::IsMethodPresent(L"Windows.Perception.Spatial.Preview.SpatialGraphInteropPreview", L"CreateLocatorForNode"))
    , m_currentDynamicNodeId()
{
}

// Transform
bool Transform::ProcessWorldTransform(Media::Payload const& payload, Windows::Perception::Spatial::SpatialCoordinateSystem const& worldOrigin)
{
    if (payload == nullptr || worldOrigin == nullptr)
    {
        return false;
    }

    hresult hr = S_OK;

    if (m_useNewApi)
    {
        hr = UpdateV2(payload, worldOrigin);
    }
    else
    {
        hr = Update(payload, worldOrigin);
    }

    return SUCCEEDED(hr);
}

// ITransform
_Use_decl_annotations_
hresult Transform::Update(
    Media::Payload const& payload,
    Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem)
{
    const auto streamSample = payload.try_as<IStreamSample>();
    if (streamSample == nullptr || streamSample->Sample() == nullptr)
    {
        IFR(MF_E_NO_VIDEO_SAMPLE_AVAILABLE);
    }

    // camera view
    UINT32 sizeCameraView = 0;
    Numerics::float4x4 cameraView{};
    IFR(streamSample->Sample()->GetBlob(MFSampleExtension_Spatial_CameraViewTransform, (UINT8*)&cameraView, sizeof(cameraView), &sizeCameraView));

    // coordinate space
    SpatialCoordinateSystem cameraCoordinateSystem = nullptr;
    IFR(streamSample->Sample()->GetUnknown(MFSampleExtension_Spatial_CameraCoordinateSystem, winrt::guid_of<SpatialCoordinateSystem>(), winrt::put_abi(cameraCoordinateSystem)));

    // sample projection matrix
    UINT32 sizeCameraProject = 0;
    Windows::Foundation::Numerics::float4x4 cameraProjection{};
    IFR(streamSample->Sample()->GetBlob(MFSampleExtension_Spatial_CameraProjectionTransform, (UINT8*)&cameraProjection, sizeof(cameraProjection), &sizeCameraProject));

    auto transformRef = cameraCoordinateSystem.TryGetTransformTo(appCoordinateSystem);
    NULL_CHK_HR(transformRef, E_POINTER);

    // transform matrix to convert to app world space
    const auto& cameraToWorld = transformRef.Value();

    // transform to world space
    Numerics::float4x4 invertedCameraView{};
    if (Numerics::invert(cameraView, &invertedCameraView))
    {
        // overwrite the cameraView with new value
        invertedCameraView *= cameraToWorld;

        streamSample->SetTransformAndProjection(invertedCameraView, cameraProjection);
    }

    return S_OK;
}

hresult Transform::UpdateV2(
    Media::Payload const& payload,
    SpatialCoordinateSystem const& worldOrigin
    
)
{
    const auto streamSample = payload.try_as<IStreamSample>();
    if (streamSample == nullptr || streamSample->Sample() == nullptr)
    {
        IFR(MF_E_INVALIDMEDIATYPE);
    }

    // get sample properties
    UINT32 sizeCameraIntrinsics = 0;
    MFPinholeCameraIntrinsics cameraIntrinsics;
    IFR(streamSample->Sample()->GetBlob(MFSampleExtension_PinholeCameraIntrinsics, (UINT8*)&cameraIntrinsics, sizeof(cameraIntrinsics), &sizeCameraIntrinsics));

    UINT32 sizeCameraExtrinsics = 0;
    MFCameraExtrinsics cameraExtrinsics;
    IFR(streamSample->Sample()->GetBlob(MFSampleExtension_CameraExtrinsics, (UINT8*)&cameraExtrinsics, sizeof(cameraExtrinsics), &sizeCameraExtrinsics));

    // query sample for calibration and validate
    if ((sizeCameraExtrinsics != sizeof(cameraExtrinsics)) ||
        (sizeCameraIntrinsics != sizeof(cameraIntrinsics)) ||
        (cameraExtrinsics.TransformCount == 0))
    {
        return MF_E_INVALIDTYPE;
    }

    // get transform from extrinsics
    const auto& calibratedTransform = cameraExtrinsics.CalibratedTransforms[0];

    // update locator cache for dynamic node
    const winrt::guid& dynamicNodeId = calibratedTransform.CalibrationId;
    if (dynamicNodeId != m_currentDynamicNodeId || m_locator == nullptr)
    {
        m_locator = SpatialGraphInteropPreview::CreateLocatorForNode(dynamicNodeId);
        NULL_CHK_HR(m_locator, MF_E_NOT_FOUND);

        m_currentDynamicNodeId = dynamicNodeId;
    }

    // compute extrinsic transform from sample data
    const auto& translation
        = make_float4x4_translation(calibratedTransform.Position.x, calibratedTransform.Position.y, calibratedTransform.Position.z);
    const auto& rotation
        = make_float4x4_from_quaternion(Numerics::quaternion{ calibratedTransform.Orientation.x, calibratedTransform.Orientation.y, calibratedTransform.Orientation.z, calibratedTransform.Orientation.w });
    const auto& cameraToLocator
        = rotation * translation;

    // get timestamp
    UINT64 sampleTimeQpc = 0;
    IFR(streamSample->Sample()->GetUINT64(MFSampleExtension_DeviceTimestamp, &sampleTimeQpc));

    TimeSpan deviceTimestamp{ sampleTimeQpc };
    auto frameTimestamp = PerceptionTimestampHelper::FromSystemRelativeTargetTime(deviceTimestamp);

    if (worldOrigin && m_locator && frameTimestamp)
    {
        // locate dynamic node with respect to appCoordinateSystem
        const auto& location = m_locator.TryLocateAtTimestamp(frameTimestamp, worldOrigin);
        NULL_CHK_HR(location, MF_E_NOT_FOUND);

        const auto& dynamicNodeToCoordinateSystem
            = make_float4x4_from_quaternion(location.Orientation()) * make_float4x4_translation(location.Position());

        // transform matrix from locator to app world space
        Windows::Foundation::Numerics::float4x4 cameraToWorld 
            = cameraToLocator * dynamicNodeToCoordinateSystem;

        // generate the older projection matrix
        streamSample->SetTransformAndProjection(
            cameraToWorld, GetProjection(cameraIntrinsics));
    }

    return S_OK;
}

void Transform::Reset()
{
    m_locator = nullptr;
    m_frameOfReference = nullptr;
}

hresult Transform::GetTransforms(
    com_ptr<IMFSample> const& sourceSample,
    Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem,
    Windows::Foundation::Numerics::float4x4& transform,
    Windows::Foundation::Numerics::float4x4& projection) try
{
    // camera view
    UINT32 sizeCameraView = 0;
    Numerics::float4x4 cameraView{};
    IFR(sourceSample->GetBlob(MFSampleExtension_Spatial_CameraViewTransform, (UINT8*)&cameraView, sizeof(cameraView), &sizeCameraView));

    // coordinate space
    SpatialCoordinateSystem cameraCoordinateSystem = nullptr;
    IFR(sourceSample->GetUnknown(MFSampleExtension_Spatial_CameraCoordinateSystem, winrt::guid_of<SpatialCoordinateSystem>(), winrt::put_abi(cameraCoordinateSystem)));

    // sample projection matrix
    UINT32 sizeCameraProject = 0;
    Windows::Foundation::Numerics::float4x4 cameraProjection{};
    IFR(sourceSample->GetBlob(MFSampleExtension_Spatial_CameraProjectionTransform, (UINT8*)&cameraProjection, sizeof(cameraProjection), &sizeCameraProject));

    auto transformRef = cameraCoordinateSystem.TryGetTransformTo(appCoordinateSystem);
    NULL_CHK_HR(transformRef, E_POINTER);

    // transform matrix to convert to app world space
    const auto& cameraToWorld = transformRef.Value();

    // transform to world space
    Numerics::float4x4 invertedCameraView{};
    if (!Numerics::invert(cameraView, &invertedCameraView))
    {
        throw_hresult(E_UNEXPECTED);
    }

    // overwrite the cameraView with new value
    invertedCameraView *= cameraToWorld;

    transform = invertedCameraView;
    projection = cameraProjection;

    return S_OK;
}
catch(hresult_error const& er)
{
    return er.code();
}
