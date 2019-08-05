// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <d3d11_1.h>
#include <DirectXMath.h>
#include "PlatformBase.h"
#include "UnityDeviceResource.h"

struct ID3D11DeviceResource
{
	virtual winrt::com_ptr<ID3D11Device> __stdcall GetDevice() = 0;
};

struct D3D11DeviceResources
	: ID3D11DeviceResource, IUnityDeviceResource
{
	D3D11DeviceResources();
	virtual ~D3D11DeviceResources();

	// ID3D11DeviceResource
	virtual winrt::com_ptr<ID3D11Device> __stdcall GetDevice() override
	{
		//std::lock_guard<slim_mutex> guard(m_mutex);
		std::shared_lock<winrt::slim_mutex> slock(m_mutex);

		return m_unityDevice;
	}

	// IUnityDeviceResource
	virtual void __stdcall ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) override;
	virtual bool __stdcall GetUsesReverseZ() override;


private:
	HRESULT InitializeResources(ID3D11Device* d3dDevice);
	void ReleaseResources();

private:
	winrt::slim_mutex m_mutex;

	winrt::com_ptr<ID3D11Device> m_unityDevice;
};
