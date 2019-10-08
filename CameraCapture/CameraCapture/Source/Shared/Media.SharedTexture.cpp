// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "Media.SharedTexture.h"
#include "Media.Functions.h"

#include <Mferror.h>
#include <winrt/windows.perception.spatial.preview.h>
#include <winrt/windows.foundation.metadata.h>

using namespace winrt;
using namespace Windows::Graphics::DirectX::Direct3D11;
using namespace Windows::Perception;
using namespace Windows::Perception::Spatial;
using namespace Windows::Perception::Spatial::Preview;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Metadata;
using namespace Windows::Foundation::Numerics;

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
EXTERN_GUID(MFSampleExtension_DeviceTimestamp, 0x8f3e35e7, 0x2dcd, 0x4887, 0x86, 0x22, 0x2a, 0x58, 0xba, 0xa6, 0x52, 0xb0);
EXTERN_GUID(MFSampleExtension_Spatial_CameraCoordinateSystem, 0x9d13c82f, 0x2199, 0x4e67, 0x91, 0xcd, 0xd1, 0xa4, 0x18, 0x1f, 0x25, 0x34);
EXTERN_GUID(MFSampleExtension_Spatial_CameraViewTransform, 0x4e251fa4, 0x830f, 0x4770, 0x85, 0x9a, 0x4b, 0x8d, 0x99, 0xaa, 0x80, 0x9b);
EXTERN_GUID(MFSampleExtension_Spatial_CameraProjectionTransform, 0x47f9fcb5, 0x2a02, 0x4f26, 0xa4, 0x77, 0x79, 0x2f, 0xdf, 0x95, 0x88, 0x6a);
#endif

HRESULT SharedTexture::Create(
    com_ptr<ID3D11Device> const d3dDevice,
    com_ptr<IMFDXGIDeviceManager> const dxgiDeviceManager,
    uint32_t width, uint32_t height,
    com_ptr<SharedTexture>& sharedTexture)
{
    NULL_CHK_HR(d3dDevice, E_INVALIDARG);
    NULL_CHK_HR(dxgiDeviceManager, E_INVALIDARG);
    if (width < 1 && height < 1)
    {
        IFR(E_INVALIDARG);
    }

    sharedTexture = nullptr;

    HANDLE deviceHandle;
    IFR(dxgiDeviceManager->OpenDeviceHandle(&deviceHandle));

    com_ptr<ID3D11Device1> mediaDevice = nullptr;
    IFR(dxgiDeviceManager->LockDevice(deviceHandle, __uuidof(ID3D11Device1), mediaDevice.put_void(), TRUE));

    // since the device is locked, unlock before we exit function
    HRESULT hr = S_OK;

    auto textureDesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM, width, height);
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    textureDesc.MipLevels = 1;
    textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;

    // create a texture
    com_ptr<ID3D11Texture2D> spTexture = nullptr;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    com_ptr<ID3D11ShaderResourceView> spSRV = nullptr;
    com_ptr<IDXGIResource1> spDXGIResource = nullptr;
    com_ptr<ID3D11Texture2D> spMediaTexture = nullptr;
    HANDLE sharedHandle = INVALID_HANDLE_VALUE;

    IDirect3DSurface mediaSurface;
    com_ptr<IMFMediaBuffer> dxgiMediaBuffer = nullptr;
    com_ptr<IMFSample> mediaSample = nullptr;

    IFG(d3dDevice->CreateTexture2D(&textureDesc, nullptr, spTexture.put()), done);

    // srv for the texture
    srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(spTexture.get(), D3D11_SRV_DIMENSION_TEXTURE2D);
    IFG(d3dDevice->CreateShaderResourceView(spTexture.get(), &srvDesc, spSRV.put()), done);

    IFG(spTexture->QueryInterface(__uuidof(IDXGIResource1), spDXGIResource.put_void()), done);

    // create shared texture 
    IFG(spDXGIResource->CreateSharedHandle(
        nullptr,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr,
        &sharedHandle), done);

    IFG(mediaDevice->OpenSharedResource1(sharedHandle, __uuidof(ID3D11Texture2D), spMediaTexture.put_void()), done);

    IFG(GetSurfaceFromTexture(spMediaTexture.get(), mediaSurface), done);

    // create a media buffer for the texture
    IFG(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), spMediaTexture.get(), 0, /*fBottomUpWhenLinear*/false, dxgiMediaBuffer.put()), done);

    // create a sample with the buffer
    IFG(MFCreateSample(mediaSample.put()), done);

    IFG(mediaSample->AddBuffer(dxgiMediaBuffer.get()), done);

    sharedTexture = make<SharedTexture>().as<SharedTexture>();
    sharedTexture->frameTextureDesc = textureDesc;
    sharedTexture->frameTexture.attach(spTexture.detach());
    sharedTexture->frameTextureSRV.attach(spSRV.detach());
    sharedTexture->sharedTextureHandle = sharedHandle;
    sharedTexture->mediaTexture.attach(spMediaTexture.detach());
    sharedTexture->mediaSurface = mediaSurface;
    sharedTexture->mediaBuffer.attach(dxgiMediaBuffer.detach());
    sharedTexture->mediaSample.attach(mediaSample.detach());

done:
    if (FAILED(hr))
    {
        if (sharedHandle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(sharedHandle);
        }
    }

    dxgiDeviceManager->UnlockDevice(deviceHandle, FALSE);

    return hr;
}

SharedTexture::SharedTexture()
    : frameTextureDesc{}
    , frameTexture(nullptr)
    , frameTextureSRV(nullptr)
    , sharedTextureHandle(INVALID_HANDLE_VALUE)
    , mediaTexture(nullptr)
    , mediaSurface(nullptr)
    , mediaBuffer(nullptr)
    , mediaSample(nullptr)
    , transforms(nullptr)
{}

SharedTexture::~SharedTexture()
{
    Reset();
}

void SharedTexture::Reset()
{
    // primary texture
    if (sharedTextureHandle != INVALID_HANDLE_VALUE)
    {
        if (CloseHandle(sharedTextureHandle))
        {
            sharedTextureHandle = INVALID_HANDLE_VALUE;
        }
    }

    mediaSample = nullptr;
    mediaBuffer = nullptr;
    mediaSurface = nullptr;
    mediaTexture = nullptr;
    frameTextureSRV = nullptr;
    frameTexture = nullptr;

    ZeroMemory(&frameTextureDesc, sizeof(CD3D11_TEXTURE2D_DESC));

    if (transforms != nullptr)
    {
        transforms.Reset();

        transforms = nullptr;
    }
}

winrt::hresult SharedTexture::UpdateTransforms(
    SpatialCoordinateSystem const& appCoordinateSystem)
{
    NULL_CHK_HR(appCoordinateSystem, E_INVALIDARG);

    if (transforms == nullptr)
    {
        transforms = CameraCapture::Media::implementation::Transform::Create(mediaSample);
    }

    return transforms.Update(appCoordinateSystem);
}

