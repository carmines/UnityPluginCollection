#include "pch.h"
#include "Media.Transform.h"
#include "Media.Transform.g.cpp"

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

_Use_decl_annotations_
CameraCapture::Media::Transform Transform::Create(
    _In_ com_ptr<IMFSample> sourceSample)
{
    auto transform = CameraCapture::Media::Transform();
    if (SUCCEEDED(transform.as<ITransformPriv>()->Initialize(sourceSample)))
    {
        return transform;
    }

    return nullptr;
}

Transform::Transform()
    : m_sample(nullptr)
    , m_useNewApi(
        ApiInformation::IsApiContractPresent(L"Windows.Foundation.UniversalApiContract", 8)
        &&
        ApiInformation::IsMethodPresent(L"Windows.Perception.Spatial.Preview.SpatialGraphInteropPreview", L"CreateLocatorForNode"))
{
}

// ITransform
_Use_decl_annotations_
hresult Transform::Initialize(
    com_ptr<IMFSample> const& sourceSample)
{
    m_sample = sourceSample;

    return S_OK;
}

_Use_decl_annotations_
hresult Transform::Update(
    com_ptr<IMFSample> const& sourceSample,
    Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem)
{
    m_sample = sourceSample;

    return Update(appCoordinateSystem);
}


// Transform
float4x4 Transform::CameraToWorldTransform()
{
    return m_cameraToWorldTransform;
}

float4x4 Transform::CameraProjectionMatrix()
{
    return m_cameraProjectionMatrix;
}

void Transform::Reset()
{
    m_locator = nullptr;
    m_frameOfReference = nullptr;
}

hresult Transform::Update(SpatialCoordinateSystem const& appCoordinateSystem)
{
    NULL_CHK_HR(appCoordinateSystem, E_INVALIDARG);

    if (m_sample == nullptr)
    {
        return MF_E_NO_VIDEO_SAMPLE_AVAILABLE;
    }

    if (m_useNewApi)
    {
        IFR(UpdateV2(appCoordinateSystem));
    }
    else
    {
        // camera view
        UINT32 sizeCameraView = 0;
        Numerics::float4x4 cameraView{};
        IFR(m_sample->GetBlob(MFSampleExtension_Spatial_CameraViewTransform, (UINT8*)&cameraView, sizeof(cameraView), &sizeCameraView));

        // coordinate space
        SpatialCoordinateSystem cameraCoordinateSystem = nullptr;
        IFR(m_sample->GetUnknown(MFSampleExtension_Spatial_CameraCoordinateSystem, winrt::guid_of<SpatialCoordinateSystem>(), winrt::put_abi(cameraCoordinateSystem)));

        // sample projection matrix
        UINT32 sizeCameraProject = 0;
        IFR(m_sample->GetBlob(MFSampleExtension_Spatial_CameraProjectionTransform, (UINT8*)&m_cameraProjectionMatrix, sizeof(m_cameraProjectionMatrix), &sizeCameraProject));

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
        }

        m_cameraToWorldTransform = invertedCameraView;
    }

    return S_OK;
}

hresult Transform::UpdateV2(SpatialCoordinateSystem const& appCoordinateSystem)
{
    UINT32 sizeCameraIntrinsics = 0;
    MFPinholeCameraIntrinsics cameraIntrinsics;
    IFR(m_sample->GetBlob(MFSampleExtension_PinholeCameraIntrinsics, (UINT8 *)&cameraIntrinsics, sizeof(cameraIntrinsics), &sizeCameraIntrinsics));

    UINT32 sizeCameraExtrinsics = 0;
    MFCameraExtrinsics cameraExtrinsics;
    IFR(m_sample->GetBlob(MFSampleExtension_CameraExtrinsics, (UINT8 *)&cameraExtrinsics, sizeof(cameraExtrinsics), &sizeCameraExtrinsics));

    UINT64 sampleTimeQpc = 0;
    IFR(m_sample->GetUINT64(MFSampleExtension_DeviceTimestamp, &sampleTimeQpc));

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
    const auto& cameraToDynamicNode
        = rotation * translation;

    // locate dynamic node with respect to appCoordinateSystem
    const auto& timestamp = PerceptionTimestampHelper::FromSystemRelativeTargetTime(TimeSpanFromQpcTicks(sampleTimeQpc));
    const auto& location = m_locator.TryLocateAtTimestamp(timestamp, appCoordinateSystem);
    NULL_CHK_HR(location, MF_E_NOT_FOUND);

    const auto& dynamicNodeToCoordinateSystem
        = make_float4x4_from_quaternion(location.Orientation()) * make_float4x4_translation(location.Position());

    // transform for camera -> app world space
    m_cameraToWorldTransform = cameraToDynamicNode * dynamicNodeToCoordinateSystem;

    // get the projection
    m_cameraProjectionMatrix = GetProjection(cameraIntrinsics);

    return S_OK;
}
