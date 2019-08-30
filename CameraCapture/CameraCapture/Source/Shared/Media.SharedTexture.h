// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <d3d11_1.h>
#include <mfapi.h>
#include <winrt/windows.foundation.numerics.h>
#include <winrt/windows.graphics.directx.direct3d11.h>
#include <winrt/windows.perception.spatial.h>

struct SharedTexture : winrt::implements<SharedTexture, winrt::Windows::Foundation::IInspectable>
{
    static HRESULT Create(
        _In_ winrt::com_ptr<ID3D11Device> const d3dDevice,
        _In_ winrt::com_ptr<IMFDXGIDeviceManager> const dxgiDeviceManager,
        _In_ uint32_t width,
        _In_ uint32_t height,
        _Out_ winrt::com_ptr<SharedTexture>& sharedTexture);

    SharedTexture();
    virtual ~SharedTexture();

    void Reset();

    HRESULT UpdateTransforms(
        _In_ winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem);

public:
	CD3D11_TEXTURE2D_DESC frameTextureDesc;
    winrt::com_ptr<ID3D11Texture2D> frameTexture;
    winrt::com_ptr<ID3D11ShaderResourceView> frameTextureSRV;
    HANDLE sharedTextureHandle;
    winrt::com_ptr<ID3D11Texture2D> mediaTexture;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface mediaSurface;
    winrt::com_ptr<IMFMediaBuffer> mediaBuffer;
    winrt::com_ptr<IMFSample> mediaSample;

    winrt::Windows::Foundation::Numerics::float4x4 cameraToWorldTransform;
    winrt::Windows::Foundation::Numerics::float4x4 cameraProjectionMatrix;

private:
	HRESULT UpdateTransformsV2(
		_In_ winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem);

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
	winrt::guid m_currentDynamicNodeId;
	winrt::Windows::Perception::Spatial::SpatialLocator m_locator{ nullptr };
	winrt::Windows::Perception::Spatial::SpatialLocatorAttachedFrameOfReference m_frameOfReference{ nullptr };
};
