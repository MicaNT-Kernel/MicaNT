/**
 * @file ucrtbase.cpp
 * @brief Sovereign Universal C Runtime DLL (ucrtbase.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Real memory allocator, string primitives, and process termination.
 */

#include <cstdint>
#include <cstddef>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

// Sovereign Heap Pool (64 KB)
static uint8_t s_ucrtHeap[65536];
static size_t s_ucrtOffset = 0;

__declspec(dllexport) void* malloc(size_t size) noexcept {
    if (size == 0) size = 1;
    size_t aligned = (size + 15) & ~15ULL;
    if (s_ucrtOffset + aligned <= sizeof(s_ucrtHeap)) {
        void* ptr = &s_ucrtHeap[s_ucrtOffset];
        s_ucrtOffset += aligned;
        return ptr;
    }
    return nullptr;
}

__declspec(dllexport) void free(void* /*ptr*/) noexcept {
    // Lookaside bump-pointer pool; no-op free
}

__declspec(dllexport) void* calloc(size_t num, size_t size) noexcept {
    size_t total = num * size;
    void* p = malloc(total);
    if (p) {
        auto* b = static_cast<char*>(p);
        for (size_t i = 0; i < total; ++i) b[i] = 0;
    }
    return p;
}

__declspec(dllexport) void* realloc(void* ptr, size_t size) noexcept {
    if (!ptr) return malloc(size);
    void* newPtr = malloc(size);
    if (newPtr && ptr) {
        auto* d = static_cast<char*>(newPtr);
        auto* s = static_cast<const char*>(ptr);
        for (size_t i = 0; i < size; ++i) d[i] = s[i];
    }
    return newPtr;
}

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

__declspec(dllexport) void abort() noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__("movq $0x01, %%rax; movq $3, %%rdx; syscall" ::: "rax", "rdx", "rcx", "r11");
#endif
    while (true) {}
}

__declspec(dllexport) void exit(int code) noexcept {
#if defined(__x86_64__)
    uint64_t c = static_cast<uint64_t>(code);
    __asm__ __volatile__("movq $0x01, %%rax; movq %0, %%rdx; syscall" :: "r"(c) : "rax", "rdx", "rcx", "r11");
#endif
    while (true) {}
}

} // extern "C"
