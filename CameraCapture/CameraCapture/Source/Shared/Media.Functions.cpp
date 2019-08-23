// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Functions.h"

#include <windows.graphics.directx.direct3d11.interop.h>

#include <mfapi.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Media::Capture;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Media::Devices;
using namespace winrt::Windows::Media::MediaProperties;
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
MediaStreamSource CreateMediaSource(
	MediaEncodingProfile const& encodingProfile)
{
	AudioStreamDescriptor audioStreamDescriptor = nullptr;
	VideoStreamDescriptor videoStreamDescriptor = nullptr;
	MediaStreamSource mediaStreamSource = nullptr;

	if (encodingProfile.Audio() != nullptr)
	{
		audioStreamDescriptor = AudioStreamDescriptor(encodingProfile.Audio());
	}

	if (encodingProfile.Video() != nullptr)
	{
		videoStreamDescriptor = VideoStreamDescriptor(encodingProfile.Video());
	}

	if (audioStreamDescriptor != nullptr)
	{
		if (videoStreamDescriptor != nullptr)
		{
			mediaStreamSource = MediaStreamSource(videoStreamDescriptor, audioStreamDescriptor);
		}
		else
		{
			mediaStreamSource = MediaStreamSource(audioStreamDescriptor);
		}
	}
	else if (videoStreamDescriptor != nullptr)
	{
		mediaStreamSource = MediaStreamSource(videoStreamDescriptor);
	}

	return mediaStreamSource;
}

_Use_decl_annotations_
HRESULT CopySample(
    GUID majorType,
    com_ptr<IMFSample> const& srcSample,
	com_ptr<IMFSample> const& dstSample)
{
    NULL_CHK_HR(srcSample, E_INVALIDARG);
    NULL_CHK_HR(dstSample, E_INVALIDARG);

    // copy sample attributes
    com_ptr<IMFAttributes> srcAttributes = nullptr;
    IFR(srcSample->QueryInterface(__uuidof(IMFAttributes), srcAttributes.put_void()));

    com_ptr<IMFAttributes> dstAttributes = nullptr;
    IFR(dstSample->QueryInterface(__uuidof(IMFAttributes), dstAttributes.put_void()));

    IFR(srcAttributes->CopyAllItems(dstAttributes.get()));

    // get the sample time, duration and flags
    LONGLONG sampleTime = 0;
    IFR(srcSample->GetSampleTime(&sampleTime));

    LONGLONG sampleDuration = 0;
    IFR(srcSample->GetSampleDuration(&sampleDuration));

    DWORD sampleFlags = 0;
    IFR(srcSample->GetSampleFlags(&sampleFlags));

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

template <typename T>
winrt::array_view<const T> from_safe_array(LPSAFEARRAY const& safeArray)
{
	auto start = reinterpret_cast<T*>(safeArray->pvData);
	auto end = start + safeArray->cbElements;
	return winrt::array_view<const T>(start, end);
}

inline winrt::Windows::Foundation::IInspectable ConvertProperty(PROPVARIANT const& var)
{
	switch (var.vt)
	{
	case VT_BOOL:
		return PropertyValue::CreateBoolean(var.boolVal);
		break;
	case VT_I1:
		return PropertyValue::CreateChar16(var.cVal);
		break;
	case VT_UI1:
		return PropertyValue::CreateUInt8(var.bVal);
		break;
	case VT_I2:
		return PropertyValue::CreateInt16(var.iVal);
		break;
	case VT_UI2:
		return PropertyValue::CreateUInt16(var.uiVal);
		break;
	case VT_I4:
	case VT_INT:
		return PropertyValue::CreateInt32(var.lVal);
		break;
	case VT_UI4:
	case VT_UINT:
		return PropertyValue::CreateInt32(var.lVal);
		break;
	case VT_I8:
		return PropertyValue::CreateInt64(var.hVal.QuadPart);
		break;
	case VT_UI8:
		return PropertyValue::CreateUInt64(var.uhVal.QuadPart);
		break;
	case VT_R4:
		return PropertyValue::CreateSingle(var.fltVal);
		break;
	case VT_R8:
		return PropertyValue::CreateDouble(var.dblVal);
		break;
	case VT_LPWSTR:
		return PropertyValue::CreateString(var.pwszVal);
		break;
	case VT_ARRAY | VT_BOOL:
		return PropertyValue::CreateBooleanArray(from_safe_array<bool>(var.parray));
		break;
	case VT_ARRAY | VT_I1:
		return PropertyValue::CreateChar16Array(from_safe_array<char16_t>(var.parray));
		break;
	case VT_ARRAY | VT_UI1:
		return PropertyValue::CreateUInt8Array(from_safe_array<uint8_t>(var.parray));
		break;
	case VT_ARRAY | VT_I2:
		return PropertyValue::CreateInt16Array(from_safe_array<int16_t>(var.parray));
		break;
	case VT_ARRAY | VT_UI2:
		return PropertyValue::CreateUInt16Array(from_safe_array<uint16_t>(var.parray));
		break;
	case VT_ARRAY | VT_I4:
	case VT_ARRAY | VT_INT:
		return PropertyValue::CreateInt32Array(from_safe_array<int32_t>(var.parray));
		break;
	case VT_ARRAY | VT_UI4:
	case VT_ARRAY | VT_UINT:
		return PropertyValue::CreateUInt32Array(from_safe_array<uint32_t>(var.parray));
		break;
	case VT_ARRAY | VT_I8:
		return PropertyValue::CreateInt64Array(from_safe_array<int64_t>(var.parray));
		break;
	case VT_ARRAY | VT_UI8:
		return PropertyValue::CreateUInt64Array(from_safe_array<uint64_t>(var.parray));
		break;
	case VT_ARRAY | VT_R4:
		return PropertyValue::CreateSingleArray(from_safe_array<float_t>(var.parray));
		break;
	case VT_ARRAY | VT_R8:
		return PropertyValue::CreateDoubleArray(from_safe_array<double_t>(var.parray));
		break;
	case VT_ARRAY | VT_LPWSTR:
		return PropertyValue::CreateStringArray(from_safe_array<hstring>(var.parray));
		break;
	}

	return nullptr;
}

HRESULT CopyAttributesToMediaStreamSample(
	com_ptr<IMFAttributes> const& attributes,
	MediaStreamSample const& sample)
{
	UINT32 cAttributes;
	IFR(attributes->GetCount(&cAttributes));

	auto propertySet = sample.ExtendedProperties();

	PROPVARIANT propVar = {};
	for (UINT32 unIndex = 0; unIndex < cAttributes; ++unIndex)
	{
		PropVariantClear(&propVar);

		GUID guidAttributeKey;
		IFR(attributes->GetItemByIndex(unIndex, &guidAttributeKey, &propVar));

		auto value = ConvertProperty(propVar);
		if (value != nullptr)
		{
			auto wrapper = propertySet.TryLookup(guidAttributeKey);
			if (wrapper == nullptr)
			{
				propertySet.Insert(guidAttributeKey, value);
			}
		}
	}

	PropVariantClear(&propVar);

	return S_OK;
}

