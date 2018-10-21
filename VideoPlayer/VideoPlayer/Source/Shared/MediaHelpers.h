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

#include <d3d11_1.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#pragma comment(lib, "mfuuid")

#include <winrt/windows.devices.enumeration.h>
#include <winrt/windows.graphics.directx.direct3d11.h>
#include <winrt/windows.media.devices.h>
#include <winrt/windows.perception.spatial.h>

struct SharedTextureBuffer
{
    static HRESULT Create(
        _In_ ID3D11Device* d3dDevice,
        _In_ IMFDXGIDeviceManager* dxgiDeviceManager,
        _In_ uint32_t width,
        _In_ uint32_t height,
        _In_ std::weak_ptr<SharedTextureBuffer> outputBuffer);

    SharedTextureBuffer();

    virtual ~SharedTextureBuffer();

    void Reset();

    CD3D11_TEXTURE2D_DESC frameTextureDesc;
    winrt::com_ptr<ID3D11Texture2D> frameTexture;
    winrt::com_ptr<ID3D11ShaderResourceView> frameTextureSRV;
    HANDLE sharedTextureHandle;
    winrt::com_ptr<ID3D11Texture2D> mediaTexture;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface mediaSurface;
    winrt::com_ptr<IMFMediaBuffer> mediaBuffer;
    winrt::com_ptr<IMFSample> mediaSample;
};

HRESULT CreateMediaDevice(
    _In_opt_ IDXGIAdapter* pDXGIAdapter,
    _COM_Outptr_ ID3D11Device** ppDevice);

HRESULT GetSurfaceFromTexture(
    _In_ ID3D11Texture2D* pTexture,
    _Out_ winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface& ppSurface);
