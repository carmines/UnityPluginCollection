// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Functions.h"

#include <mfapi.h>
#include <mferror.h>
#include <inspectable.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/windows.perception.spatial.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Media::Capture;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Media::Devices;
using namespace winrt::Windows::Media::MediaProperties;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Perception::Spatial;

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
EXTERN_GUID(MFSampleExtension_DeviceTimestamp, 0x8f3e35e7, 0x2dcd, 0x4887, 0x86, 0x22, 0x2a, 0x58, 0xba, 0xa6, 0x52, 0xb0);
EXTERN_GUID(MFSampleExtension_Spatial_CameraCoordinateSystem, 0x9d13c82f, 0x2199, 0x4e67, 0x91, 0xcd, 0xd1, 0xa4, 0x18, 0x1f, 0x25, 0x34);
EXTERN_GUID(MFSampleExtension_Spatial_CameraViewTransform, 0x4e251fa4, 0x830f, 0x4770, 0x85, 0x9a, 0x4b, 0x8d, 0x99, 0xaa, 0x80, 0x9b);
EXTERN_GUID(MFSampleExtension_Spatial_CameraProjectionTransform, 0x47f9fcb5, 0x2a02, 0x4f26, 0xa4, 0x77, 0x79, 0x2f, 0xdf, 0x95, 0x88, 0x6a);
#endif

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

    // copy IMFAttributes
    IFR(srcSample->CopyAllItems(dstSample.get()));

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
            IFR(srcSample->ConvertToContiguousBuffer(srcBuffer.put())); // GPU -> CPU
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

        // QI
        auto srcBuffer2D = srcBuffer.as<IMF2DBuffer2>();
        auto dstBuffer2D = dstBuffer.as<IMF2DBuffer2>();

        // copy the media buffer
        IFR(srcBuffer2D->Copy2DTo(dstBuffer2D.get()));
    }

    return S_OK;
}

template <typename T>
array_view<const T> from_safe_array(LPSAFEARRAY const& safeArray)
{
    auto start = reinterpret_cast<T*>(safeArray->pvData);
    auto end = start + safeArray->cbElements;
    return array_view<const T>(start, end);
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
    _In_ com_ptr<IMFSample> const& mediaSample,
    _In_ MediaStreamSample const& streamSample)
{
    UINT32 cAttributes;
    IFR(mediaSample->GetCount(&cAttributes));

    auto propertySet = streamSample.ExtendedProperties();

    PROPVARIANT propVar = {};
    for (UINT32 unIndex = 0; unIndex < cAttributes; ++unIndex)
    {
        PropVariantClear(&propVar);

        GUID guidAttributeKey;
        IFR(mediaSample->GetItemByIndex(unIndex, &guidAttributeKey, &propVar));

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

    // HoloLens specific properties to check for and add
    UINT32 sizeCameraView = 0;
    Numerics::float4x4 cameraView{};
    if (SUCCEEDED(mediaSample->GetBlob(MFSampleExtension_Spatial_CameraViewTransform, (UINT8*)&cameraView, sizeof(cameraView), &sizeCameraView)))
    {
        auto iinspectable = propertySet.TryLookup(MFSampleExtension_Spatial_CameraViewTransform);
        if (iinspectable != nullptr)
        {
            propertySet.Remove(MFSampleExtension_Spatial_CameraViewTransform);
        }
        propertySet.Insert(MFSampleExtension_Spatial_CameraViewTransform, box_value(cameraView));
    }

    // coordinate space
    SpatialCoordinateSystem cameraCoordinateSystem = nullptr;
    if (SUCCEEDED(mediaSample->GetUnknown(MFSampleExtension_Spatial_CameraCoordinateSystem, winrt::guid_of<SpatialCoordinateSystem>(), winrt::put_abi(cameraCoordinateSystem))))
    {
        auto iinspectable = propertySet.TryLookup(MFSampleExtension_Spatial_CameraCoordinateSystem);
        if (iinspectable != nullptr)
        {
            propertySet.Remove(MFSampleExtension_Spatial_CameraCoordinateSystem);
        }
        propertySet.Insert(MFSampleExtension_Spatial_CameraCoordinateSystem, box_value(cameraCoordinateSystem));
    }

    // projection matrix
    UINT32 sizeCameraProject = 0;
    Numerics::float4x4 cameraProjectionMatrix{};
    if (SUCCEEDED(mediaSample->GetBlob(MFSampleExtension_Spatial_CameraProjectionTransform, (UINT8*)&cameraProjectionMatrix, sizeof(cameraProjectionMatrix), &sizeCameraProject)))
    {
        auto iinspectable = propertySet.TryLookup(MFSampleExtension_Spatial_CameraProjectionTransform);
        if (iinspectable != nullptr)
        {
            propertySet.Remove(MFSampleExtension_Spatial_CameraProjectionTransform);
        }
        propertySet.Insert(MFSampleExtension_Spatial_CameraProjectionTransform, box_value(cameraProjectionMatrix));
    }

    // intrinsics
    UINT32 sizeCameraIntrinsics = 0;
    MFPinholeCameraIntrinsics cameraIntrinsics{};
    if (SUCCEEDED(mediaSample->GetBlob(MFSampleExtension_PinholeCameraIntrinsics, (UINT8 *)&cameraIntrinsics, sizeof(cameraIntrinsics), &sizeCameraIntrinsics)))
    {
        auto iinspectable = propertySet.TryLookup(MFSampleExtension_PinholeCameraIntrinsics);
        if (iinspectable != nullptr)
        {
            propertySet.Remove(MFSampleExtension_PinholeCameraIntrinsics);
        }

        auto start = reinterpret_cast<uint8_t*>(&cameraIntrinsics);
        auto end = start + sizeCameraIntrinsics;
        iinspectable = PropertyValue::CreateUInt8Array(array_view<const uint8_t>(start, end));

        propertySet.Insert(MFSampleExtension_PinholeCameraIntrinsics, iinspectable);
    }

    // extrinsices
    UINT32 sizeCameraExtrinsics = 0;
    MFCameraExtrinsics cameraExtrinsics{};
    if (SUCCEEDED(mediaSample->GetBlob(MFSampleExtension_CameraExtrinsics, (UINT8 *)&cameraExtrinsics, sizeof(cameraExtrinsics), &sizeCameraExtrinsics)))
    {
        auto iinspectable = propertySet.TryLookup(MFSampleExtension_CameraExtrinsics);
        if (iinspectable != nullptr)
        {
            propertySet.Remove(MFSampleExtension_CameraExtrinsics);
        }

        auto start = reinterpret_cast<uint8_t*>(&cameraExtrinsics);
        auto end = start + sizeCameraExtrinsics;
        iinspectable = PropertyValue::CreateUInt8Array(array_view<const uint8_t>(start, end));

        propertySet.Insert(MFSampleExtension_CameraExtrinsics, iinspectable);
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT GetDXGISurfaceFromSample(
    com_ptr<IMFSample> const& mediaSample,
    com_ptr<IDXGISurface2>& surface)
{
    // validate it has only one buffer
    DWORD bufferCount = 0;
    IFR(mediaSample->GetBufferCount(&bufferCount));

    if (bufferCount > 1)
    {
        IFR(MF_E_INVALIDTYPE); 
    }

    com_ptr<IMFMediaBuffer> mediaBuffer = nullptr;
    IFR(mediaSample->GetBufferByIndex(0, mediaBuffer.put()));

    com_ptr<IMFDXGIBuffer> mediaDxgiBuffer = nullptr;
    IFR(mediaBuffer->QueryInterface(__uuidof(IMFDXGIBuffer), mediaDxgiBuffer.put_void()));

    com_ptr<IDXGISurface2> dxgiSurface = nullptr;
    if (FAILED(mediaDxgiBuffer->GetResource(__uuidof(IDXGISurface2), dxgiSurface.put_void()))) // TEXTURE2D
    {
        com_ptr<IDXGIResource1> dxgiResource = nullptr;
        IFR(mediaDxgiBuffer->GetResource(__uuidof(IDXGIResource1), dxgiResource.put_void())); // TEXTURE2DARRAY

        UINT subResource = 0;
        IFR(mediaDxgiBuffer->GetSubresourceIndex(&subResource));

        IFR(dxgiResource->CreateSubresourceSurface(subResource, dxgiSurface.put()));
    }

    surface.attach(dxgiSurface.detach());

    return S_OK;
}

_Use_decl_annotations_
HRESULT CreateMediaStreamSample(
    com_ptr<IMFSample> const& mediaSample, 
    TimeSpan const& timeStamp, 
    MediaStreamSample& streamSample)
{
    MediaStreamSample mediaStreamSample = nullptr;

    com_ptr<IDXGISurface2> dxgiSurface = nullptr;
    if (SUCCEEDED(GetDXGISurfaceFromSample(mediaSample, dxgiSurface)))
    {
        com_ptr<::IInspectable> surface = nullptr;
        IFR(CreateDirect3D11SurfaceFromDXGISurface(dxgiSurface.get(), surface.put()));

        auto d3dSurace = surface.as<IDirect3DSurface>();

        mediaStreamSample = MediaStreamSample::CreateFromDirect3D11Surface(d3dSurace, timeStamp);
    }
    else
    {
        com_ptr<IMFMediaBuffer> mediaBuffer;
        IFR(mediaSample->ConvertToContiguousBuffer(mediaBuffer.put()));

        winrt::Windows::Storage::Streams::IBuffer sampleBuffer = nullptr;

        DWORD bufferLength = 0;
        BYTE *srcBuffer = nullptr;
        if (SUCCEEDED(mediaBuffer->Lock(&srcBuffer, nullptr, &bufferLength)))
        {
            sampleBuffer = make<CustomBuffer>(bufferLength); // TODO: make a pool of buffers and resize if required

            auto bufferByteAccess = sampleBuffer.as<IBufferByteAccess>();

            BYTE *dstBuffer = nullptr;
            if SUCCEEDED(bufferByteAccess->Buffer(&dstBuffer))
            {
                CopyMemory(dstBuffer, srcBuffer, bufferLength);

                sampleBuffer.Length(bufferLength);
            }

            IFR(mediaBuffer->Unlock());
        }

        mediaStreamSample = MediaStreamSample::CreateFromBuffer(sampleBuffer, timeStamp);
    }

    IFR(CopyAttributesToMediaStreamSample(mediaSample, mediaStreamSample));

    // Set MediaStream Properties
    LONGLONG sampleDuration = 0;
    IFR(mediaSample->GetSampleDuration(&sampleDuration));
    mediaStreamSample.Duration(TimeSpan{ sampleDuration });

    UINT32 isDiscontinuous = false;
    if (FAILED(mediaSample->GetUINT32(MFSampleExtension_Discontinuity, &isDiscontinuous)))
    {
        isDiscontinuous = false;
    }
    mediaStreamSample.Discontinuous(static_cast<bool>(isDiscontinuous));

    UINT32 isKeyFrame = false;
    if (FAILED(mediaSample->GetUINT32(MFSampleExtension_CleanPoint, &isKeyFrame)))
    {
        isKeyFrame = false;
    }
    mediaStreamSample.KeyFrame(static_cast<bool>(isKeyFrame));

    UINT64 sampleTimeQpc = 0;
    if (SUCCEEDED(mediaSample->GetUINT64(MFSampleExtension_DeviceTimestamp, &sampleTimeQpc)))
    {
        mediaStreamSample.DecodeTimestamp(TimeSpan{ sampleTimeQpc });
    }

    streamSample = mediaStreamSample;

    return S_OK;
}

_Use_decl_annotations_
HRESULT CopyToTargetTexture(
    com_ptr<ID3D11Device> const& d3dDevice,
    com_ptr<ID3D11DeviceContext> const& d3dDeviceContext,
    com_ptr<ID3D11Texture2D> const& source,
    DXGI_COLOR_SPACE_TYPE const inputColorSpace, 
    com_ptr<ID3D11Texture2D> const& target, 
    DXGI_COLOR_SPACE_TYPE const targetColorSpace)
{
    auto videoDevice = d3dDevice.as<ID3D11VideoDevice>();
    auto videoContext = d3dDeviceContext.as<ID3D11VideoContext1>();

    D3D11_TEXTURE2D_DESC sourceDesc{};
    source->GetDesc(&sourceDesc);

    D3D11_TEXTURE2D_DESC destDesc{};
    target->GetDesc(&destDesc);

    // create enumerator
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC vpcdesc{};
    vpcdesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    vpcdesc.InputFrameRate.Numerator = 60;
    vpcdesc.InputFrameRate.Denominator = 1;
    vpcdesc.InputWidth = sourceDesc.Width;
    vpcdesc.InputHeight = sourceDesc.Height;
    vpcdesc.OutputFrameRate.Numerator = 60;
    vpcdesc.OutputFrameRate.Denominator = 1;
    vpcdesc.OutputWidth = destDesc.Width;
    vpcdesc.OutputHeight = destDesc.Height;
    vpcdesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL; // D3D11_VIDEO_USAGE_OPTIMAL_SPEED or D3D11_VIDEO_USAGE_OPTIMAL_QUALITY

    com_ptr<ID3D11VideoProcessorEnumerator> videoProcEnum = nullptr;
    IFR(videoDevice->CreateVideoProcessorEnumerator(&vpcdesc, videoProcEnum.put()));

    // create processor
    com_ptr<ID3D11VideoProcessor> videoProc = nullptr;
    IFR(videoDevice->CreateVideoProcessor(videoProcEnum.get(), 0, videoProc.put()));
    videoContext->VideoProcessorSetStreamFrameFormat(videoProc.get(), 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);

    // set colorspaces
    videoContext->VideoProcessorSetStreamColorSpace1(videoProc.get(), 0, inputColorSpace);
    videoContext->VideoProcessorSetOutputColorSpace1(videoProc.get(), targetColorSpace);

    // 
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc{};
    inputDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inputDesc.Texture2D.MipSlice = 0;
    inputDesc.Texture2D.ArraySlice = 0;

    com_ptr<ID3D11VideoProcessorInputView> inputView = nullptr;
    IFR(videoDevice->CreateVideoProcessorInputView(source.get(), videoProcEnum.get(), &inputDesc, inputView.put()));

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc{};
    outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outputDesc.Texture2D.MipSlice = 0;

    com_ptr<ID3D11VideoProcessorOutputView> outputView = nullptr;
    IFR(videoDevice->CreateVideoProcessorOutputView(target.get(), videoProcEnum.get(), &outputDesc, outputView.put()));

    // blit
    D3D11_VIDEO_PROCESSOR_STREAM vpStream{};
    vpStream.Enable = TRUE;
    vpStream.pInputSurface = inputView.get();
    IFR(videoContext->VideoProcessorBlt(videoProc.get(), outputView.get(), 0, 1, &vpStream));

    return S_OK;
}

HRESULT NV12ToRGB(
    _In_ byte* pSrcBuffer,
    _In_ byte* pDstBuffer,
    _In_ int height,
    _In_ int width,
    _In_ int stride,
    _In_ bool yFlip)
{
    NULL_CHK_HR(pSrcBuffer, E_INVALIDARG);
    NULL_CHK_HR(pDstBuffer, E_INVALIDARG);

    // TODO: bounds checks on buffers, should add size of buffers to function

    const int numBytes = 4; // for RGBA
    const bool bgra = false; // change to true to flip color values

    for (int y = 0; y < height; y++)
    {
        auto yRow = yFlip ? pSrcBuffer + (height - 1 - y) * width : pSrcBuffer + y * stride;

        auto dstRow = pDstBuffer + y * width * numBytes;

        auto uvRow = pSrcBuffer + (height + (y >> 1)) * stride;
        for (int x = 0; x < width; x++)
        {
            auto uvIndex = (x >> 1) << 1;

            // Get YUV
            byte Y = yRow[x];
            byte U = uvRow[uvIndex + 0];
            byte V = uvRow[uvIndex + 1];

            // Conversion formula from http://msdn.microsoft.com/en-us/library/ms893078
            // coefficients 
            uint32_t C = Y - 16;
            uint32_t D = U - 128;
            uint32_t E = V - 128;

            // intermediate results
            uint32_t R = (298 * C           + 409 * E + 128) >> 8;
            uint32_t G = (298 * C - 100 * D - 208 * E + 128) >> 8;
            uint32_t B = (298 * C + 516 * D           + 128) >> 8;

            // set values
            dstRow[0] = std::clamp<byte>(static_cast<byte>(bgra ? B : R), 0, 255);
            dstRow[1] = std::clamp<byte>(static_cast<byte>(G), 0, 255);
            dstRow[2] = std::clamp<byte>(static_cast<byte>(bgra ? R : B), 0, 255);
            if constexpr (numBytes == 4)
            {
                dstRow[3] = 0xff;
            }

            // next pixel
            dstRow += numBytes;
        }
    }

    return S_OK;
}
