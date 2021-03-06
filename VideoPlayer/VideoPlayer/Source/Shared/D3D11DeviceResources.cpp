// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "D3D11DeviceResources.h"
#include "Unity/IUnityGraphicsD3D11.h"
#include <d3d11_1.h>

using namespace winrt;

std::shared_ptr<IUnityDeviceResource> __stdcall CreateD3D11DeviceResource()
{
	return std::make_shared<D3D11DeviceResources>();
}

D3D11DeviceResources::D3D11DeviceResources()
	: m_unityDevice(nullptr)
{
}

D3D11DeviceResources::~D3D11DeviceResources()
{
}

_Use_decl_annotations_
void D3D11DeviceResources::ProcessDeviceEvent(
	UnityGfxDeviceEventType type,
	IUnityInterfaces* interfaces)
{
	std::lock_guard<slim_mutex> guard(m_mutex);

	switch (type)
	{
	case kUnityGfxDeviceEventInitialize:
	{
		IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
		if (d3d != nullptr)
		{
			auto device = d3d->GetDevice();
			if (device != nullptr)
			{
				IFV(InitializeResources(device));
			}
		}

		break;
	}
	case kUnityGfxDeviceEventShutdown:
	{
		ReleaseResources();

		break;
	}
	case kUnityGfxDeviceEventBeforeReset:
	{
		break;
	}
	case kUnityGfxDeviceEventAfterReset:
	{
		break;
	}
	}
}

bool D3D11DeviceResources::GetUsesReverseZ()
{
	std::shared_lock<slim_mutex> slock(m_mutex);

	return (int)m_unityDevice->GetFeatureLevel() >= (int)D3D_FEATURE_LEVEL_10_0;
}

HRESULT D3D11DeviceResources::InitializeResources(ID3D11Device* d3dDevice)
{
	d3dDevice->AddRef();
	m_unityDevice.attach(d3dDevice);

	return S_OK;
}

void D3D11DeviceResources::ReleaseResources()
{
	if (m_unityDevice != nullptr)
	{
		m_unityDevice = nullptr;
	}
}
