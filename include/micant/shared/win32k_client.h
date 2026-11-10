#pragma once

#include <stdint.h>

#if defined(MICANT_WIN32K_EXPORTS)
#define MICANT_WIN32K_API __declspec(dllexport)
#elif defined(MICANT_WIN32K_IMPORTS)
#define MICANT_WIN32K_API __declspec(dllimport)
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
typedef struct MicaWin32kWindowClassEx {
    uint32_t cbSize, style;
    void* wndProc;
    int32_t classExtra, windowExtra;
    void* instance;
    void* icon;
    void* cursor;
    void* background;
    const wchar_t* menuName;
    const wchar_t* className;
    void* smallIcon;
} MicaWin32kWindowClassEx;
typedef struct MicaWin32kCreateStruct {
    void* createParams;
    void* instance;
    void* menu;
    void* parent;
    int32_t height, width, y, x;
    int32_t style;
    const wchar_t* title;
    const wchar_t* className;
    uint32_t exStyle;
} MicaWin32kCreateStruct;
typedef struct MicaWin32kBitmapInfoHeader {
    uint32_t size;
    int32_t width, height;
    uint16_t planes, bitCount;
    uint32_t compression, imageSize;
    int32_t xPixelsPerMeter, yPixelsPerMeter;
    uint32_t colorsUsed, colorsImportant;
} MicaWin32kBitmapInfoHeader;

MICANT_WIN32K_API void* MicaWin32kCreateWindowExW(uint32_t, const wchar_t*, const wchar_t*, uint32_t,
    int32_t, int32_t, int32_t, int32_t, void*, void*, void*, void*);
MICANT_WIN32K_API int32_t MicaWin32kDestroyWindow(void*);
MICANT_WIN32K_API int32_t MicaWin32kShowWindow(void*, int32_t);
MICANT_WIN32K_API int32_t MicaWin32kSetWindowPos(void*, void*, int32_t, int32_t, int32_t, int32_t, uint32_t);
MICANT_WIN32K_API int32_t MicaWin32kPostMessageW(void*, uint32_t, uintptr_t, intptr_t);
MICANT_WIN32K_API intptr_t MicaWin32kSendMessageW(void*, uint32_t, uintptr_t, intptr_t);
MICANT_WIN32K_API void MicaWin32kPostQuitMessage(int32_t);
MICANT_WIN32K_API uint16_t MicaWin32kRegisterClassW(const wchar_t*, void*);
MICANT_WIN32K_API intptr_t MicaWin32kDispatchMessageW(const MicaWin32kMessage*);
MICANT_WIN32K_API intptr_t MicaWin32kSetWindowLongPtrW(void*, int32_t, intptr_t);
MICANT_WIN32K_API intptr_t MicaWin32kGetWindowLongPtrW(void*, int32_t);
MICANT_WIN32K_API intptr_t MicaWin32kDefWindowProcW(void*, uint32_t, uintptr_t, intptr_t);
MICANT_WIN32K_API void* MicaWin32kGetDC(void*);
MICANT_WIN32K_API int32_t MicaWin32kReleaseDC(void*, void*);
MICANT_WIN32K_API void* MicaWin32kCreateCompatibleDC(void*);
MICANT_WIN32K_API void* MicaWin32kCreateCompatibleBitmap(void*, int32_t, int32_t);
MICANT_WIN32K_API void* MicaWin32kSelectObject(void*, void*);
MICANT_WIN32K_API int32_t MicaWin32kBitBlt(void*, int32_t, int32_t, int32_t, int32_t, void*, int32_t, int32_t, uint32_t);
MICANT_WIN32K_API int32_t MicaWin32kDeleteObject(void*);
MICANT_WIN32K_API void* MicaWin32kGetStockObject(int32_t);
MICANT_WIN32K_API void* MicaWin32kCreateDibSection(void*, int32_t, int32_t, void**);
MICANT_WIN32K_API int32_t MicaWin32kPeekMessageW(MicaWin32kMessage*, void*, uint32_t, uint32_t, uint32_t);
MICANT_WIN32K_API int32_t MicaWin32kGetMessageW(MicaWin32kMessage*, void*, uint32_t, uint32_t);
MICANT_WIN32K_API int32_t MicaWin32kGetClientRect(void*, MicaWin32kRect*);
MICANT_WIN32K_API int32_t MicaWin32kGetWindowRect(void*, MicaWin32kRect*);

#ifdef __cplusplus
}
#endif
