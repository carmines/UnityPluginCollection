// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include <memory>
#include <shared_mutex>

#define WIN32_LEAN_AND_MEAN

// cppwrint
#include <unknwn.h>
#include <winrt/base.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/Windows.storage.streams.h>

#include <windows.h>
#include <strsafe.h>

#pragma comment(lib, "mfuuid")

struct __declspec(uuid("905a0fef-bc53-11df-8c49-001e4fc686da")) IBufferByteAccess : ::IUnknown
{
    virtual HRESULT __stdcall Buffer(uint8_t** value) = 0;
};

struct CustomBuffer : winrt::implements<CustomBuffer, winrt::Windows::Storage::Streams::IBuffer, ::IBufferByteAccess>
{
    std::vector<uint8_t> m_buffer;
    uint32_t m_length{};

    CustomBuffer(uint32_t size) :
        m_buffer(size)
    {
    }

    uint32_t Capacity() const
    {
        return m_buffer.size();
    }

    uint32_t Length() const
    {
        return m_length;
    }

    void Length(uint32_t value)
    {
        if (value > m_buffer.size())
        {
            throw winrt::hresult_invalid_argument();
        }

        m_length = value;
    }

    HRESULT __stdcall Buffer(uint8_t** value) final
    {
        *value = m_buffer.data();
        return S_OK;
    }
};


struct CriticalSection
{
    struct CriticalSectionGuard
    {
        CriticalSectionGuard(CRITICAL_SECTION& criticalSection) 
            : m_criticalSection(criticalSection)
        {
            ::EnterCriticalSection(&m_criticalSection);
        }

        ~CriticalSectionGuard()
        {
            ::LeaveCriticalSection(&m_criticalSection);
        }

    private:
        CriticalSectionGuard(const CriticalSectionGuard&) = delete;
        CriticalSectionGuard& operator=(const CriticalSectionGuard&) = delete;

        CRITICAL_SECTION& m_criticalSection;
    };

    CriticalSection()
    {
        InitializeCriticalSection(&m_cs);
    }

    ~CriticalSection()
    {
        DeleteCriticalSection(&m_cs);
    }

    CriticalSectionGuard const Guard() { return CriticalSectionGuard(m_cs); }

private:
    CRITICAL_SECTION m_cs;
};

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
#define IFR(HR) { HRESULT hrTest = HR; if (FAILED(hrTest)) { return hrTest; } } 
#endif

#ifndef NULL_CHK_HR
#define NULL_CHK_HR(pointer, HR) if (nullptr == pointer) { IFR(HR); }
#endif

#ifndef IFV
#define IFV(HR) { HRESULT hrTest = HR; if (FAILED(hrTest)) { return; } }
#endif

#ifndef IFT
#define IFT(HR) { HRESULT hrTest = HR; if (FAILED(hrTest)) { winrt::throw_hresult(hrTest); } }
#endif

#ifndef IFG
#define IFG(HR, marker) { HRESULT hrTest = HR; if (FAILED(hrTest)) { hr = hrTest; goto marker; } }
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
    PreviewVideoFrame,
    PhotoFrame
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

extern "C" typedef void(__stdcall *StateChangedCallback)(_In_ void* callbackObject, _In_ CALLBACK_STATE args);
