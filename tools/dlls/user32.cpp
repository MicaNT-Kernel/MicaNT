/**
 * @file user32.cpp
 * @brief Sovereign User Windowing Subsystem DLL (user32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * ZERO direct syscall instructions: dynamically chains to ntdll.dll via real PE32+ IAT!
 */

#include <cstdint>
#include <cstddef>

extern "C" {

__declspec(dllimport) uint64_t NtYieldExecution() noexcept;

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

#pragma pack(push, 1)
struct Point {
    int32_t x{0};
    int32_t y{0};
};

struct Msg {
    void*    hwnd{nullptr};
    uint32_t message{0};
    uint64_t wParam{0};
    int64_t  lParam{0};
    uint32_t time{0};
    Point    pt{};
    uint32_t lPrivate{0};
};
#pragma pack(pop)

static bool s_quitPosted = false;
static int s_quitExitCode = 0;
static uint64_t s_windowHandleCounter = 0x1000;

__declspec(dllexport) int32_t GetSystemMetrics(int32_t nIndex) noexcept {
    switch (nIndex) {
        case 0:  return 1920; // SM_CXSCREEN
        case 1:  return 1080; // SM_CYSCREEN
        case 4:  return 32;   // SM_CYCAPTION
        case 80: return 1;    // SM_CMONITORS
        default: return 0;
    }
}

__declspec(dllexport) void* CreateWindowExW(
    uint32_t /*dwExStyle*/,
    const wchar_t* /*lpClassName*/,
    const wchar_t* /*lpWindowName*/,
    uint32_t /*dwStyle*/,
    int /*X*/,
    int /*Y*/,
    int /*nWidth*/,
    int /*nHeight*/,
    void* /*hWndParent*/,
    void* /*hMenu*/,
    void* /*hInstance*/,
    void* /*lpParam*/
) noexcept {
    return reinterpret_cast<void*>(++s_windowHandleCounter);
}

__declspec(dllexport) bool DestroyWindow(void* /*hWnd*/) noexcept {
    return true;
}

__declspec(dllexport) bool ShowWindow(void* /*hWnd*/, int /*nCmdShow*/) noexcept {
    return true;
}

__declspec(dllexport) bool UpdateWindow(void* /*hWnd*/) noexcept {
    return true;
}

__declspec(dllexport) bool GetMessageW(
    Msg* lpMsg,
    void* /*hWnd*/,
    uint32_t /*wMsgFilterMin*/,
    uint32_t /*wMsgFilterMax*/
) noexcept {
    if (!lpMsg) return false;
    if (s_quitPosted) {
        lpMsg->message = 0x0012; // WM_QUIT
        lpMsg->wParam = static_cast<uint64_t>(s_quitExitCode);
        return false;
    }
    NtYieldExecution();
    lpMsg->message = 0x000F; // WM_PAINT
    lpMsg->hwnd = reinterpret_cast<void*>(s_windowHandleCounter);
    return true;
}

__declspec(dllexport) bool PeekMessageW(
    Msg* lpMsg,
    void* /*hWnd*/,
    uint32_t /*wMsgFilterMin*/,
    uint32_t /*wMsgFilterMax*/,
    uint32_t /*wRemoveMsg*/
) noexcept {
    if (!lpMsg) return false;
    if (s_quitPosted) {
        lpMsg->message = 0x0012; // WM_QUIT
        lpMsg->wParam = static_cast<uint64_t>(s_quitExitCode);
        return true;
    }
    return false;
}

__declspec(dllexport) bool TranslateMessage(const Msg* /*lpMsg*/) noexcept {
    return false;
}

__declspec(dllexport) int64_t DispatchMessageW(const Msg* /*lpMsg*/) noexcept {
    return 0;
}

__declspec(dllexport) int64_t DefWindowProcW(
    void* /*hWnd*/,
    uint32_t /*Msg*/,
    uint64_t /*wParam*/,
    int64_t /*lParam*/
) noexcept {
    return 0;
}

__declspec(dllexport) void PostQuitMessage(int nExitCode) noexcept {
    s_quitPosted = true;
    s_quitExitCode = nExitCode;
}

__declspec(dllexport) void* GetDC(void* /*hWnd*/) noexcept {
    return reinterpret_cast<void*>(0x2001);
}

__declspec(dllexport) int ReleaseDC(void* /*hWnd*/, void* /*hDC*/) noexcept {
    return 1;
}

__declspec(dllexport) int MessageBoxW(
    void* /*hWnd*/,
    const wchar_t* /*lpText*/,
    const wchar_t* /*lpCaption*/,
    uint32_t /*uType*/
) noexcept {
    return 1; // IDOK
}

__declspec(dllexport) int MessageBoxA(
    void* /*hWnd*/,
    const char* /*lpText*/,
    const char* /*lpCaption*/,
    uint32_t /*uType*/
) noexcept {
    return 1; // IDOK
}

} // extern "C"
