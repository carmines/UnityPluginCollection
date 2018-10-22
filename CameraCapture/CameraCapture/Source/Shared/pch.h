// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#define WIN32_LEAN_AND_MEAN

// cppwrint
#include <winrt/base.h>
#include <winrt/windows.foundation.h>

#include <windows.h>
#include <strsafe.h>

#pragma comment(lib, "mfuuid")

inline void __stdcall Log(
    _In_ _Printf_format_string_ STRSAFE_LPCWSTR pszFormat,
    ...)
{
    wchar_t szTextBuf[2048];

    va_list args;
    va_start(args, pszFormat);

    StringCchVPrintf(szTextBuf, _countof(szTextBuf), pszFormat, args);

    va_end(args);

    OutputDebugStringW(szTextBuf);
}

#ifndef IFR
#define IFR(hresult) { HRESULT hrTest = hresult; if (FAILED(hrTest)) { return hrTest; } } 
#endif

#ifndef NULL_CHK_HR
#define NULL_CHK_HR(pointer, hresult) if (nullptr == pointer) { IFR(hresult); }
#endif

#ifndef IFV
#define IFV(hresult) { HRESULT hrTest = hresult; if (FAILED(hrTest)) { return; } }
#endif

#ifndef IFT
#define IFT(hresult) { HRESULT hrTest = hresult; if (FAILED(hrTest)) { winrt::throw_hresult(hrTest); } }
#endif

#ifndef IFG
#define IFG(hresult, marker) { HRESULT hrTest = hresult; if (FAILED(hrTest)) { hr = hrTest; goto marker; } }
#endif

#ifndef NULL_CHK_R
#define NULL_CHK_R(pointer) if (nullptr == pointer) { return; }
#endif

typedef int32_t INSTANCE_HANDLE;

#ifndef INSTANCE_HANDLE_INVALID
#define INSTANCE_HANDLE_INVALID static_cast<INSTANCE_HANDLE>(0x0bad)
#define INSTANCE_HANDLE_START static_cast<INSTANCE_HANDLE>(0x0bae)
#endif // INSTANCE_HANDLE_INVALID

typedef enum class _CallbackType : int32_t
{
    None = 0,
    Failed,
    Capture,
} CallbackType;

typedef struct _FAILED_STATE
{
    int32_t hresult;
} FAILED_STATE;

typedef enum class _CaptureStateType : int32_t
{
    None = 0,
    PreviewStarted,
    PreviewStopped,
    PreviewAudioFrame,
    PreviewVideoFrame
} CaptureStateType;

typedef struct _CAPTURE_STATE
{
    CaptureStateType stateType;
    int32_t width;
    int32_t height;
    void* texturePtr;
    winrt::Windows::Foundation::Numerics::float4x4 worldMatrix;
    winrt::Windows::Foundation::Numerics::float4x4 projectionMatrix;
} CAPTURE_STATE;

#pragma pack(push, 4)
typedef struct _CALLBACK_STATE
{
    CallbackType type;
    union 
    {
        FAILED_STATE failedState;
        CAPTURE_STATE captureState;
    } value;
} CALLBACK_STATE;
#pragma pack(pop)

extern "C" typedef void(__stdcall *StateChangedCallback)(_In_ CALLBACK_STATE args);
