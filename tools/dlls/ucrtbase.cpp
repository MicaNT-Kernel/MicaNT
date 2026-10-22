/**
 * @file ucrtbase.cpp
 * @brief Sovereign Universal C Runtime DLL (ucrtbase.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 */

#include <cstdint>
#include <cstddef>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept { return true; }

__declspec(dllexport) void* malloc(size_t) noexcept { return nullptr; }
__declspec(dllexport) void free(void*) noexcept {}
__declspec(dllexport) void* realloc(void*, size_t) noexcept { return nullptr; }
__declspec(dllexport) void* calloc(size_t, size_t) noexcept { return nullptr; }

__declspec(dllexport) void* memcpy(void* dest, const void* src, size_t n) noexcept {
    auto d = static_cast<char*>(dest);
    auto s = static_cast<const char*>(src);
    while (n--) *d++ = *s++;
    return dest;
}

__declspec(dllexport) void* memset(void* dest, int c, size_t n) noexcept {
    auto d = static_cast<char*>(dest);
    while (n--) *d++ = static_cast<char>(c);
    return dest;
}

__declspec(dllexport) int memcmp(const void* s1, const void* s2, size_t n) noexcept {
    auto p1 = static_cast<const unsigned char*>(s1);
    auto p2 = static_cast<const unsigned char*>(s2);
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

__declspec(dllexport) size_t strlen(const char* s) noexcept {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

__declspec(dllexport) int strcmp(const char* s1, const char* s2) noexcept {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

__declspec(dllexport) void abort() noexcept { while (true) {} }
__declspec(dllexport) void exit(int) noexcept { while (true) {} }

} // extern "C"
