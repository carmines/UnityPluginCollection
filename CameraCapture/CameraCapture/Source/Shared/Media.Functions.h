// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <d3d11_1.h>
#include <mfidl.h>

#include <winrt/windows.foundation.h>
#include <winrt/windows.devices.enumeration.h>
#include <winrt/windows.media.devices.h>
#include <winrt/windows.media.mediaproperties.h>
#include <winrt/windows.media.capture.h>
#include <winrt/windows.graphics.directx.direct3d11.h>

HRESULT CreateMediaDevice(
    _In_opt_ IDXGIAdapter* pDXGIAdapter,
    _COM_Outptr_ ID3D11Device** ppDevice);

winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Devices::Enumeration::DeviceInformation> GetFirstDeviceAsync(
    _In_ winrt::Windows::Devices::Enumeration::DeviceClass const& deviceClass);

winrt::Windows::Media::MediaProperties::IMediaEncodingProperties GetVideoDeviceProperties(
    _In_ winrt::Windows::Media::Devices::VideoDeviceController const& videoDeviceController,
    _In_ winrt::Windows::Media::Capture::MediaStreamType mediaStreamType,
    _In_ uint32_t width,
    _In_ uint32_t height,
    _In_ winrt::hstring subType);

HRESULT GetSurfaceFromTexture(
    _In_ ID3D11Texture2D* pTexture,
    _Out_ winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface& ppSurface);

HRESULT CopySample(
    _In_ GUID majorType,
    _In_ IMFSample* srcSample,
    _In_ IMFSample* dstSample);
