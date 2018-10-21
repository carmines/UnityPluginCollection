// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Plugin.PdfLoader.h"
#include "WICTextureLoader.h"
#include <winrt/windows.storage.streams.h>

using namespace winrt;
using namespace PDFLoader::Plugin::implementation;
using namespace Windows::Foundation;
using namespace Windows::Data::Pdf;
using namespace Windows::Storage::Streams;

using winrtPdfLoader = PDFLoader::Plugin::PdfLoader;

_Use_decl_annotations_
PDFLoader::Plugin::IModule PdfLoader::Create(
    std::weak_ptr<IUnityDeviceResource> const& unityDevice,
    StateChangedCallback fnCallback)
{
    auto loader = make<PdfLoader>();

    if (SUCCEEDED(loader.as<IModulePriv>()->Initialize(unityDevice, fnCallback)))
    {
        return loader;
    }

    return nullptr;
}

PdfLoader::PdfLoader()
    : m_loadDataAsyncOp(nullptr)
    , m_selectPageAsync(nullptr)
    , m_document(nullptr)
    , m_page(nullptr)
    , m_pageTexture(nullptr)
    , m_pageTextureSRV(nullptr) {
}

// IModule
void PdfLoader::Shutdown()
{
    if (m_loadDataAsyncOp != nullptr)
    {
        m_loadDataAsyncOp.Cancel();
        m_loadDataAsyncOp = nullptr;
    }

    if (m_selectPageAsync != nullptr)
    {
        m_selectPageAsync.Cancel();
        m_selectPageAsync = nullptr;
    }

    if (m_page != nullptr)
    {
        m_page.Close();
        m_page = nullptr;
    }

    if (m_document != nullptr)
    {
        m_document = nullptr;
    }

    Module::Shutdown();
}

// PdfLoader
HRESULT PdfLoader::LoadFile(hstring const& folderName, hstring const& fileName)
{
    NULL_CHK_HR(fileName.c_str(), E_INVALIDARG);

    if (m_loadDataAsyncOp != nullptr && m_loadDataAsyncOp.Status() == AsyncStatus::Started)
    {
        return hresult_canceled().code();
    }

    m_loadDataAsyncOp = LoadFileAsync(folderName, fileName);
    if (m_loadDataAsyncOp.Status() != AsyncStatus::Completed)
    {
        m_loadDataAsyncOp.Completed([=](auto const& asyncOp, AsyncStatus const& status)
        {
            UNREFERENCED_PARAMETER(asyncOp);

            if (status == AsyncStatus::Canceled)
            {
                return;
            }

            CALLBACK_STATE state{};
            ZeroMemory(&state, sizeof(CALLBACK_STATE));

            if (status == AsyncStatus::Error)
            {
                state.type = CallbackType::Failed;

                ZeroMemory(&state.value.failedState, sizeof(FAILED_STATE));

                state.value.failedState.hresult = to_hresult();
            }
            else
            {
                // notify plugin of the mesh vertex and index size
                state.type = CallbackType::Pdf;

                ZeroMemory(&state.value.pdfState, sizeof(PDF_STATE));
                state.value.pdfState.stateType = PdfStateType::Opened;
                state.value.pdfState.page = 0;
                state.value.pdfState.width = 0;
                state.value.pdfState.height = 0;
                state.value.pdfState.textureSRV = nullptr;
            }

            Callback(state);
        });

        m_loadDataAsyncOp.Progress([&](auto&&, double progress)
        {
            CALLBACK_STATE callbackState;
            ZeroMemory(&callbackState, sizeof(callbackState));

            // notify plugin of the mesh vertex and index size
            callbackState.type = CallbackType::Pdf;

            ZeroMemory(&callbackState.value.pdfState, sizeof(PDF_STATE));
            callbackState.value.pdfState.stateType = progress < 1.0 ? PdfStateType::Loading : PdfStateType::Loaded;
            callbackState.value.pdfState.page = 0;
            callbackState.value.pdfState.width = 0;
            callbackState.value.pdfState.height = 0;
            callbackState.value.pdfState.textureSRV = nullptr;

            Callback(callbackState);
        });
    }

    return S_OK;
}

HRESULT PdfLoader::SelectPage(uint32_t pageIndex)
{
    NULL_CHK_HR(m_document, E_NOT_VALID_STATE);

    if (m_selectPageAsync != nullptr && m_selectPageAsync.Status() == AsyncStatus::Started)
    {
        return hresult_canceled().code();
    }

    m_selectPageAsync = SelectPageAsync(pageIndex);
    if (m_selectPageAsync.Status() != AsyncStatus::Completed)
    {
        m_selectPageAsync.Completed([=](auto const& asyncOp, AsyncStatus const& status)
        {
            UNREFERENCED_PARAMETER(asyncOp);

            if (status == AsyncStatus::Canceled)
            {
                return;
            }

            CALLBACK_STATE state{};
            ZeroMemory(&state, sizeof(CALLBACK_STATE));

            if (status == AsyncStatus::Error)
            {
                state.type = CallbackType::Failed;

                ZeroMemory(&state.value.failedState, sizeof(FAILED_STATE));

                state.value.failedState.hresult = to_hresult();
            }
            else
            {
                // notify plugin of the mesh vertex and index size
                state.type = CallbackType::Pdf;

                ZeroMemory(&state.value.pdfState, sizeof(PDF_STATE));
                state.value.pdfState.stateType = PdfStateType::Selected;

                auto trimbox = m_page.Dimensions().TrimBox();

                state.value.pdfState.page = pageIndex;
                state.value.pdfState.width = static_cast<int32_t>(trimbox.Width);
                state.value.pdfState.height = static_cast<int32_t>(trimbox.Height);
                state.value.pdfState.textureSRV = m_pageTextureSRV.get();
            }

            Callback(state);
        });
    }

    return S_OK;
}

// internal
IAsyncActionWithProgress<double> PdfLoader::LoadFileAsync(hstring folderName, hstring fileName)
{
    auto context = apartment_context();

    co_await resume_background();

    auto progress = co_await get_progress_token();

    double currentProgress = 0;
    double total = 4;

    progress(0.0);

    Windows::Storage::StorageFolder storageFolder = nullptr;
    try
    {
        storageFolder = Windows::Storage::ApplicationData::Current().LocalFolder();
    }
    catch (...)
    {
        storageFolder = Windows::Storage::KnownFolders::DocumentsLibrary();
    }

    if (!folderName.empty() && std::wstring(folderName).find(L":\\") != std::wstring::npos)
    {
        storageFolder = co_await Windows::Storage::StorageFolder::GetFolderFromPathAsync(folderName);
    }
    else if (!folderName.empty())
    {
        storageFolder = co_await storageFolder.GetFolderAsync(folderName);
    }

    progress(++currentProgress / total);

    auto storageFile = co_await storageFolder.GetFileAsync(fileName);

    progress(++currentProgress / total);

    auto stream = co_await storageFile.OpenAsync(Windows::Storage::FileAccessMode::Read);

    progress(++currentProgress / total);

    m_document = co_await PdfDocument::LoadFromStreamAsync(stream);

    progress(++currentProgress / total);

    co_await context;

    co_return;
}

IAsyncAction PdfLoader::SelectPageAsync(uint32_t pageIndex)
{
    m_page = m_document.GetPage(pageIndex);

    auto size = m_page.Size();
    co_await m_page.PreparePageAsync();

    InMemoryRandomAccessStream memStream;
    auto renderOptions = PdfPageRenderOptions();
    renderOptions.DestinationHeight(2048);
    renderOptions.DestinationWidth(2048);
    co_await m_page.RenderToStreamAsync(memStream, renderOptions);

    com_ptr<IStream> spStream = nullptr;
    IFT(CreateStreamOverRandomAccessStream(winrt::get_unknown(memStream), __uuidof(IStream), spStream.put_void()));

    auto pWIC = DirectX::_GetWIC();

    com_ptr<IWICStream> stream;
    IFT(pWIC->CreateStream(stream.put()));

    IFT(stream->InitializeFromIStream(spStream.get()));

    com_ptr<IWICBitmapDecoder> decoder;
    IFT(pWIC->CreateDecoderFromStream(stream.get(), 0, WICDecodeMetadataCacheOnDemand, decoder.put()));

    com_ptr<IWICBitmapFrameDecode> frame;
    IFT(decoder->GetFrame(0, frame.put()));

    UINT width = 0, height = 0;
    IFT(frame->GetSize(&width, &height));

    auto resources = m_deviceResources.lock();
    if (resources != nullptr)
    {
        com_ptr<ID3D11DeviceResource> spD3D11Resources = nullptr;
        IFT(resources->QueryInterface(__uuidof(ID3D11DeviceResource), spD3D11Resources.put_void()));

        com_ptr<ID3D11Resource> texture;
        com_ptr<ID3D11ShaderResourceView> textureSRV;
        IFT(DirectX::CreateTextureFromWIC(
            spD3D11Resources->GetDevice().get(), nullptr,
            frame.get(), 0,
            D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, DirectX::WIC_LOADER_DEFAULT,
            texture.put(), textureSRV.put(),
            true));

        m_pageTexture = texture;
        m_pageTextureSRV = textureSRV;
    }
    else
    {
        throw_hresult(E_NOT_VALID_STATE);
    }
}
