/**
 * @file gdi32.cpp
 * @brief Sovereign Graphics Device Interface DLL (gdi32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Connects standard GDI raster ops directly to MicaNT VRAM presentation (SSN 0x81).
 */

#include <cstdint>
#include <cstddef>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

static uint64_t s_gdiHandleCounter = 0x3000;

__declspec(dllexport) void* CreateCompatibleDC(void* /*hdc*/) noexcept {
    return reinterpret_cast<void*>(++s_gdiHandleCounter);
}

__declspec(dllexport) bool DeleteDC(void* /*hdc*/) noexcept {
    return true;
}

__declspec(dllexport) void* CreateCompatibleBitmap(void* /*hdc*/, int /*cx*/, int /*cy*/) noexcept {
    return reinterpret_cast<void*>(++s_gdiHandleCounter);
}

__declspec(dllexport) void* CreateSolidBrush(uint32_t /*color*/) noexcept {
    return reinterpret_cast<void*>(++s_gdiHandleCounter);
}

__declspec(dllexport) void* SelectObject(void* /*hdc*/, void* hgdiobj) noexcept {
    return hgdiobj;
}

__declspec(dllexport) bool DeleteObject(void* /*ho*/) noexcept {
    return true;
}

__declspec(dllexport) void* GetStockObject(int /*i*/) noexcept {
    return reinterpret_cast<void*>(0x10); // Standard WHITE_BRUSH / SYSTEM_FONT
}

#pragma pack(push, 1)
struct UserPresentSurfaceParams64 {
    uint64_t surfaceBufferVa{0};
    uint32_t dstX{0};
    uint32_t dstY{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t pitch{0};
    uint32_t reserved{0};
};
#pragma pack(pop)

__declspec(dllexport) bool BitBlt(
    void* /*hdcDest*/,
    int xDest,
    int yDest,
    int w,
    int h,
    void* /*hdcSrc*/,
    int /*xSrc*/,
    int /*ySrc*/,
    uint32_t /*rop*/
) noexcept {
    if (w <= 0 || h <= 0) return true;
    UserPresentSurfaceParams64 params;
    params.surfaceBufferVa = 0; // Kernel fallback or direct VRAM
    params.dstX = static_cast<uint32_t>(xDest >= 0 ? xDest : 0);
    params.dstY = static_cast<uint32_t>(yDest >= 0 ? yDest : 0);
    params.width = static_cast<uint32_t>(w);
    params.height = static_cast<uint32_t>(h);
    params.pitch = params.width;
    params.reserved = 0;

#if defined(__x86_64__)
    uint64_t p = reinterpret_cast<uint64_t>(&params);
    __asm__ __volatile__(
        "movq %0, %%rcx\n\t"
        "movq %0, %%r10\n\t"
        "movq $0x81, %%rax\n\t"
        "movq %0, %%rdx\n\t"
        "syscall\n\t"
        :
        : "r"(p)
        : "rax", "rdx", "r8", "rcx", "r10", "r11", "memory"
    );
#endif
    return true;
}

__declspec(dllexport) bool TextOutW(
    void* /*hdc*/,
    int /*x*/,
    int /*y*/,
    const wchar_t* /*lpString*/,
    int /*c*/
) noexcept {
    return true;
}

__declspec(dllexport) bool TextOutA(
    void* /*hdc*/,
    int /*x*/,
    int /*y*/,
    const char* /*lpString*/,
    int /*c*/
) noexcept {
    return true;
}

} // extern "C"
