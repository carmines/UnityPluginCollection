#pragma once

#include <d3d11_1.h>
#include <mfapi.h>
#include <winrt/windows.foundation.numerics.h>
#include <winrt/windows.graphics.directx.direct3d11.h>
#include <winrt/windows.perception.spatial.h>

struct SharedTexture : winrt::implements<SharedTexture, winrt::Windows::Foundation::IInspectable>
{
    static HRESULT Create(
        _In_ ID3D11Device* d3dDevice,
        _In_ IMFDXGIDeviceManager* dxgiDeviceManager,
        _In_ uint32_t width,
        _In_ uint32_t height,
        _Out_ winrt::com_ptr<SharedTexture>& sharedTexture);

    SharedTexture();
    virtual ~SharedTexture();

    void Reset();

    HRESULT UpdateTransforms(
        _In_ winrt::Windows::Perception::Spatial::SpatialCoordinateSystem const& appCoordinateSystem);

    CD3D11_TEXTURE2D_DESC frameTextureDesc;
    winrt::com_ptr<ID3D11Texture2D> frameTexture;
    winrt::com_ptr<ID3D11ShaderResourceView> frameTextureSRV;
    HANDLE sharedTextureHandle;
    winrt::com_ptr<ID3D11Texture2D> mediaTexture;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface mediaSurface;
    winrt::com_ptr<IMFMediaBuffer> mediaBuffer;
    winrt::com_ptr<IMFSample> mediaSample;

    winrt::Windows::Foundation::Numerics::float4x4 cameraToWorldMatrix;
    winrt::Windows::Foundation::Numerics::float4x4 cameraViewInWorldMatrix;
    winrt::Windows::Foundation::Numerics::float4x4 cameraViewMatrix;
    winrt::Windows::Foundation::Numerics::float4x4 cameraProjectionMatrix;
};
