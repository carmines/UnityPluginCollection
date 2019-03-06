// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#define WIN32_LEAN_AND_MEAN

// cppwrint
#include <winrt/base.h>
#include <winrt/windows.foundation.h>

#ifndef IFR
#define IFR(hresult) { HRESULT hrTest = hresult; if (FAILED(hrTest)) { return hrTest; } } 
#endif

#ifndef NULL_CHK_HR
#define NULL_CHK_HR(pointer, hresult) if (nullptr == pointer) { IFR(hresult); }
#endif

#ifndef IFV
#define IFV(hresult) { HRESULT hrTest = hresult; if (FAILED(hrTest)) { return; } }
#endif

#ifndef IFG
#define IFG(result, marker) { HRESULT hrTest = result; if (FAILED(hrTest)) { hr = hrTest; goto marker; } }
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
    VideoPlayer
} CallbackType;

typedef struct _FAILED_STATE
{
    int32_t hresult;
} FAILED_STATE;

typedef enum class _MediaPlayerState : int32_t
{
    None = 0,
    Opening,
    Buffering,
    Playing,
    Paused,
    Opened,
    Ended,
} MediaPlayerState;

typedef struct _PLAYBACK_STATE
{
    MediaPlayerState state;
    int32_t width;
    int32_t height;
    boolean canSeek;
    int64_t duration;
} PLAYBACK_STATE;

#pragma pack(push, 4)
typedef struct _CALLBACK_STATE
{
    CallbackType type;
    union 
    {
        FAILED_STATE failedState;
        PLAYBACK_STATE playbackState;
    } value;
} CALLBACK_STATE;
#pragma pack(pop)

extern "C" typedef void(__stdcall *StateChangedCallback)(_In_ void* callbackObject, _In_ CALLBACK_STATE args);
