#pragma once

#include <stdint.h>

#if defined(MICANT_WIN32K_EXPORTS)
#define MICANT_WIN32K_API __declspec(dllexport)
#else
#define MICANT_WIN32K_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MicaWin32kRect { int32_t left, top, right, bottom; } MicaWin32kRect;
typedef struct MicaWin32kPoint { int32_t x, y; } MicaWin32kPoint;
typedef struct MicaWin32kMessage {
    void* hwnd;
    uint32_t message;
    uintptr_t wParam;
    intptr_t lParam;
    uint32_t time;
    MicaWin32kPoint pt;
} MicaWin32kMessage;

MICANT_WIN32K_API void* MicaWin32kCreateWindowExW(uint32_t, const wchar_t*, const wchar_t*, uint32_t,
    int32_t, int32_t, int32_t, int32_t, void*, void*, void*, void*);
MICANT_WIN32K_API int32_t MicaWin32kDestroyWindow(void*);
MICANT_WIN32K_API int32_t MicaWin32kShowWindow(void*, int32_t);
MICANT_WIN32K_API int32_t MicaWin32kSetWindowPos(void*, void*, int32_t, int32_t, int32_t, int32_t, uint32_t);
MICANT_WIN32K_API int32_t MicaWin32kPeekMessageW(MicaWin32kMessage*, void*, uint32_t, uint32_t, uint32_t);
MICANT_WIN32K_API int32_t MicaWin32kGetMessageW(MicaWin32kMessage*, void*, uint32_t, uint32_t);
MICANT_WIN32K_API int32_t MicaWin32kGetClientRect(void*, MicaWin32kRect*);
MICANT_WIN32K_API int32_t MicaWin32kGetWindowRect(void*, MicaWin32kRect*);

#ifdef __cplusplus
}
#endif
