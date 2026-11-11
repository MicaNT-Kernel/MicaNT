#include "micant/shared/win32k_client.h"

#include <cassert>
#include <cstdint>

int main() {
    constexpr uint32_t srcCopy = 0x00CC0020;
    void* memory = MicaWin32kCreateCompatibleDC(nullptr);
    void* other = MicaWin32kCreateCompatibleDC(nullptr);
    assert(memory && other);

    void* bits = nullptr;
    void* bitmap = MicaWin32kCreateDibSection(nullptr, 2, 2, &bits);
    assert(bitmap && bits);
    auto* pixels = static_cast<uint32_t*>(bits);
    pixels[0] = 0x12345678;
    void* defaultBitmap = MicaWin32kSelectObject(memory, bitmap);
    assert(defaultBitmap && defaultBitmap != bitmap);
    assert(!MicaWin32kDeleteObject(bitmap));
    assert(!MicaWin32kSelectObject(other, bitmap));
    assert(!MicaWin32kSelectObject(other, defaultBitmap));
    assert(MicaWin32kSelectObject(memory, defaultBitmap) == bitmap);
    assert(MicaWin32kSelectObject(other, bitmap));
    assert(!MicaWin32kDeleteObject(bitmap));
    assert(MicaWin32kDeleteDC(other));
    assert(!MicaWin32kDeleteDC(other));
    assert(MicaWin32kDeleteObject(bitmap));
    assert(!MicaWin32kDeleteObject(bitmap));

    void* window = reinterpret_cast<void*>(static_cast<uintptr_t>(1));
    void* screen = MicaWin32kGetDC(window);
    assert(screen);
    assert(!MicaWin32kDeleteDC(screen));
    assert(!MicaWin32kReleaseDC(nullptr, screen));
    assert(!MicaWin32kReleaseDC(window, memory));
    assert(MicaWin32kReleaseDC(window, screen));
    assert(!MicaWin32kReleaseDC(window, screen));
    assert(!MicaWin32kBitBlt(screen, 0, 0, 1, 1, memory, 0, 0, srcCopy));

    assert(MicaWin32kDeleteDC(memory));
    for (unsigned i = 0; i < 128; ++i) {
        void* dc = MicaWin32kCreateCompatibleDC(nullptr);
        assert(dc);
        assert(MicaWin32kDeleteDC(dc));
        dc = MicaWin32kGetDC(nullptr);
        assert(dc);
        assert(MicaWin32kReleaseDC(nullptr, dc));
    }
}
