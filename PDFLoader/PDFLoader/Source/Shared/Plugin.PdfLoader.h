// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Plugin/PdfLoader.g.h"
#include "Plugin.Module.h"

namespace winrt::PDFLoader::Plugin::implementation
{
    struct PdfLoader : PdfLoaderT<PdfLoader, PDFLoader::Plugin::implementation::Module>
    {
        static IModule Create(
            _In_ std::weak_ptr<IUnityDeviceResource> const& unityDevice,
            _In_ StateChangedCallback fnCallback);

        PdfLoader();

        // IModule
        virtual void Shutdown();

        HRESULT LoadFile(hstring const& folderName, hstring const& fileName);
        HRESULT SelectPage(uint32_t pageIndex);

        Windows::Data::Pdf::PdfDocument Document() { return m_document; }
        Windows::Data::Pdf::PdfPage Page() { return m_page; }
        uint32_t PageCount() { return (m_document != nullptr) ? m_document.PageCount() : 0; }

    private:
        Windows::Foundation::IAsyncActionWithProgress<double> LoadFileAsync(hstring folderName, hstring fileName);
        Windows::Foundation::IAsyncAction SelectPageAsync(uint32_t pageIndex);

    private:
        Windows::Foundation::IAsyncActionWithProgress<double> m_loadDataAsyncOp;
        Windows::Foundation::IAsyncAction m_selectPageAsync;

        Windows::Data::Pdf::PdfDocument m_document;
        Windows::Data::Pdf::PdfPage m_page;
        com_ptr<ID3D11Resource> m_pageTexture;
        com_ptr<ID3D11ShaderResourceView> m_pageTextureSRV;
    };
}

namespace winrt::PDFLoader::Plugin::factory_implementation
{
    struct PdfLoader : PdfLoaderT<PdfLoader, implementation::PdfLoader>
    {
    };
}
