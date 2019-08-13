// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Functions.h"

#include <windows.graphics.directx.direct3d11.interop.h>

#include <mfapi.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Media::MediaProperties;
using namespace winrt::Windows::Media::Devices;
using namespace winrt::Windows::Media::Capture;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

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
IAsyncOperation<DeviceInformation> GetFirstDeviceAsync(
    DeviceClass const& deviceClass)
{
    DeviceInformation deviceInfo = nullptr;

    // get collection for device class
    auto devices = co_await DeviceInformation::FindAllAsync(deviceClass);
    for (auto const& device : devices)
    {
        Log(L"Name: %s, Id: %s\n", device.Name().c_str(), device.Id().c_str());
        if (deviceInfo == nullptr)
        {
            deviceInfo = device;
        }
    }

    co_return deviceInfo;
}

_Use_decl_annotations_
IMediaEncodingProperties GetVideoDeviceProperties(
    VideoDeviceController const& videoDeviceController,
    MediaStreamType mediaStreamType,
    uint32_t width,
    uint32_t height,
    hstring subType)
{
	// select a camera property that meets the resolution at the highest framerate
	auto preferredSettings = videoDeviceController.GetAvailableMediaStreamProperties(mediaStreamType);
	if (preferredSettings.Size() == 0)
	{
		return nullptr;
	}

	Log(L"Total available MediaStreamProperties for %s: %i\n", videoDeviceController.Id().c_str(), preferredSettings.Size());

	setlocale(LC_ALL, "");

	IMediaEncodingProperties mediaEncodingProperty = nullptr;
	auto const& found = std::find_if(begin(preferredSettings), end(preferredSettings), [&](IMediaEncodingProperties const& prop)
		{
			// validate it is video
			if (prop.Type() != L"Video")
			{
				false;
			}

			auto videoProperty = prop.as<IVideoEncodingProperties>();

			if (mediaEncodingProperty == nullptr)
			{
				mediaEncodingProperty = videoProperty;
			}

			Log(L"\tFormat: %s: %i x %i @ %d/%d fps",
				prop.Subtype().c_str(),
				videoProperty.Width(),
				videoProperty.Height(),
				videoProperty.FrameRate().Numerator(),
				videoProperty.FrameRate().Denominator());

			// select a size that will be == width/height @ 30fps, final size will be set with enc props
			double fps = videoProperty.FrameRate().Numerator() / videoProperty.FrameRate().Denominator();
			bool match =
				_wcsicmp(videoProperty.Subtype().c_str(), subType.c_str()) == 0 &&
				videoProperty.Width() == width &&
				videoProperty.Height() == height &&
				fps == 30.0;
			if (match)
			{
				Log(L" - found\n");
			}
			else
			{
				Log(L"\n");
			}

			return match;
		});

	if (found != end(preferredSettings))
	{
		mediaEncodingProperty = *found;
	}

	return mediaEncodingProperty;
}

_Use_decl_annotations_
HRESULT GetSurfaceFromTexture(
    ID3D11Texture2D* pTexture,
    IDirect3DSurface& direct3dSurface)
{
    com_ptr<IDXGISurface> dxgiSurface = nullptr;
    IFR(pTexture->QueryInterface(__uuidof(IDXGISurface), dxgiSurface.put_void()));

    com_ptr<::IInspectable> spInspectable = nullptr;
    IFR(CreateDirect3D11SurfaceFromDXGISurface(dxgiSurface.get(), spInspectable.put()));

    IFR(spInspectable->QueryInterface(
        guid_of<IDirect3DSurface>(),
        put_abi(direct3dSurface)));

    return S_OK;
}

_Use_decl_annotations_
HRESULT CopySample(
    GUID majorType,
    IMFSample *srcSample,
    IMFSample *dstSample)
{
    NULL_CHK_HR(srcSample, E_INVALIDARG);
    NULL_CHK_HR(dstSample, E_INVALIDARG);

    // get the sample time, duration and flags
    LONGLONG sampleTime = 0;
    IFR(srcSample->GetSampleTime(&sampleTime));

    LONGLONG sampleDuration = 0;
    IFR(srcSample->GetSampleDuration(&sampleDuration));

    DWORD sampleFlags = 0;
    IFR(srcSample->GetSampleFlags(&sampleFlags));

    // copy sample attributes
    com_ptr<IMFAttributes> srcAttributes = nullptr;
    IFR(srcSample->QueryInterface(__uuidof(IMFAttributes), srcAttributes.put_void()));

    com_ptr<IMFAttributes> dstAttributes = nullptr;
    IFR(dstSample->QueryInterface(__uuidof(IMFAttributes), dstAttributes.put_void()));

    IFR(srcAttributes->CopyAllItems(dstAttributes.get()));

    // set time, duration and flags
    IFR(dstSample->SetSampleTime(sampleTime));
    IFR(dstSample->SetSampleDuration(sampleDuration));
    IFR(dstSample->SetSampleFlags(sampleFlags));

    if (MFMediaType_Audio == majorType)
    {
        com_ptr<IMFMediaBuffer> dstBuffer = nullptr;
        IFR(dstSample->GetBufferByIndex(0, dstBuffer.put()));

        IFR(srcSample->CopyToBuffer(dstBuffer.get()));
    }
    else if (MFMediaType_Video == majorType)
    {
        DWORD srcBufferCount = 0;
        IFR(srcSample->GetBufferCount(&srcBufferCount));

        com_ptr<IMFMediaBuffer> srcBuffer = nullptr;
        if (srcBufferCount > 1)
        {
            IFR(srcSample->ConvertToContiguousBuffer(srcBuffer.put()));
        }
        else
        {
            IFR(srcSample->GetBufferByIndex(0, srcBuffer.put()));
        }

        DWORD dstBufferCount = 0;
        IFR(dstSample->GetBufferCount(&dstBufferCount));

        com_ptr<IMFMediaBuffer> dstBuffer = nullptr;
        if (dstBufferCount > 1)
        {
            IFR(dstSample->ConvertToContiguousBuffer(dstBuffer.put()));
        }
        else
        {
            IFR(dstSample->GetBufferByIndex(0, dstBuffer.put()));
        }

        // QI for MF2DBuffer2
        com_ptr<IMF2DBuffer2> srcBuffer2D = nullptr;
        IFR(srcBuffer->QueryInterface(__uuidof(IMF2DBuffer2), srcBuffer2D.put_void()));

        com_ptr<IMF2DBuffer2> dstBuffer2D = nullptr;
        IFR(dstBuffer->QueryInterface(__uuidof(IMF2DBuffer2), dstBuffer2D.put_void()));

        // copy the media buffer
        IFR(srcBuffer2D->Copy2DTo(dstBuffer2D.get()));
    }

    return S_OK;
}
