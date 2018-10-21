//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include "pch.h"
#include "MediaHelpers.h"

#include <windows.graphics.directx.direct3d11.interop.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Media::Playback;

HRESULT SharedTextureBuffer::Create(
    _In_ ID3D11Device* d3dDevice,
    _In_ IMFDXGIDeviceManager* dxgiDeviceManager,
    _In_ uint32_t width,
    _In_ uint32_t height,
    _In_ std::weak_ptr<SharedTextureBuffer> outputBuffer)
{
    NULL_CHK_HR(d3dDevice, E_INVALIDARG);
    NULL_CHK_HR(dxgiDeviceManager, E_INVALIDARG);
    if (width < 1 && height < 1)
    {
        IFR(E_INVALIDARG);
    }

    auto buffer = outputBuffer.lock();
    NULL_CHK_HR(buffer, E_INVALIDARG);

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

    buffer->frameTextureDesc = textureDesc;
    buffer->frameTexture.attach(spTexture.detach());
    buffer->frameTextureSRV.attach(spSRV.detach());
    buffer->sharedTextureHandle = sharedHandle;
    buffer->mediaTexture.attach(spMediaTexture.detach());
    buffer->mediaSurface = mediaSurface;
    buffer->mediaBuffer.attach(dxgiMediaBuffer.detach());
    buffer->mediaSample.attach(mediaSample.detach());

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

SharedTextureBuffer::SharedTextureBuffer()
    : frameTextureDesc{}
    , frameTexture(nullptr)
    , frameTextureSRV(nullptr)
    , sharedTextureHandle(INVALID_HANDLE_VALUE)
    , mediaTexture(nullptr)
    , mediaSurface(nullptr)
    , mediaBuffer(nullptr)
    , mediaSample(nullptr)
{}

SharedTextureBuffer::~SharedTextureBuffer()
{
    Reset();
}

void SharedTextureBuffer::Reset()
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
}

// Check for SDK Layer support.
inline bool SdkLayersAvailable()
{
    HRESULT hr = E_NOTIMPL;

#if defined(_DEBUG)
    hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
        0,
        D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
        nullptr,                    // Any feature level will do.
        0,
        D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
        nullptr,                    // No need to keep the D3D device reference.
        nullptr,                    // No need to know the feature level.
        nullptr                     // No need to keep the D3D device context reference.
    );
#endif

    return SUCCEEDED(hr);
}

_Use_decl_annotations_
HRESULT CreateMediaDevice(
    IDXGIAdapter* pDXGIAdapter, 
    ID3D11Device** ppDevice)
{
    NULL_CHK_HR(ppDevice, E_INVALIDARG);

    // Create the Direct3D 11 API device object and a corresponding context.
    D3D_FEATURE_LEVEL featureLevel;

    // This flag adds support for surfaces with a different color channel ordering
    // than the API default. It is required for compatibility with Direct2D.
    UINT creationFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    // If the project is in a debug build, enable debugging via SDK Layers with this flag.
    if (SdkLayersAvailable())
    {
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
    }

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    com_ptr<ID3D11Device> spDevice;
    com_ptr<ID3D11DeviceContext> spContext;

    D3D_DRIVER_TYPE driverType = (nullptr != pDXGIAdapter) ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

    // Create a device using the hardware graphics driver if adapter is not supplied
    HRESULT hr = D3D11CreateDevice(
        pDXGIAdapter,               // if nullptr will use default adapter.
        driverType, 
        0,                          // Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
        creationFlags,              // Set debug and Direct2D compatibility flags.
        featureLevels,              // List of feature levels this app can support.
        ARRAYSIZE(featureLevels),   // Size of the list above.
        D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
        spDevice.put(),             // Returns the Direct3D device created.
        &featureLevel,              // Returns feature level of device created.
        spContext.put()             // Returns the device immediate context.
    );

    if (FAILED(hr))
    {
        // fallback to WARP if we are not specifying an adapter
        if (nullptr == pDXGIAdapter)
        {
            // If the initialization fails, fall back to the WARP device.
            // For more information on WARP, see: 
            // http://go.microsoft.com/fwlink/?LinkId=286690
            hr = D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
                0,
                creationFlags,
                featureLevels,
                ARRAYSIZE(featureLevels),
                D3D11_SDK_VERSION,
                spDevice.put(),
                &featureLevel,
                spContext.put());
        }

        IFR(hr);
    }
    else
    {
        // workaround for nvidia GPU's, cast to ID3D11VideoDevice
        auto videoDevice = spDevice.as<ID3D11VideoDevice>();
    }

    // Turn multithreading on 
    auto spMultithread = spContext.as<ID3D10Multithread>();
    NULL_CHK_HR(spMultithread, E_POINTER);

    spMultithread->SetMultithreadProtected(TRUE);

    *ppDevice = spDevice.detach();

    return S_OK;
}

_Use_decl_annotations_
HRESULT GetSurfaceFromTexture(
    ID3D11Texture2D* pTexture,
    IDirect3DSurface& direct3dSurface)
{
    com_ptr<IDXGISurface> dxgiSurface = nullptr;
    IFR(pTexture->QueryInterface(__uuidof(IDXGISurface),  dxgiSurface.put_void()));
    
    com_ptr<::IInspectable> spInspectable = nullptr;
    IFR(CreateDirect3D11SurfaceFromDXGISurface(dxgiSurface.get(), spInspectable.put()));

    IFR(spInspectable->QueryInterface(
        guid_of<IDirect3DSurface>(),
        reinterpret_cast<void**>(put_abi(direct3dSurface))));

    return S_OK;
}
