// Minimal freestanding image used to verify the file-backed SEC_IMAGE path.
// It has no imports and no DllMain, so the Ps loader can map it as a main
// image without the DLL-initializer support that M4 does not provide. The
// self-referential global forces a base relocation entry, so the loader can
// also exercise the relocated view path.
extern "C" {
volatile const void* g_selfReference = &g_selfReference;

[[noreturn]] void _start() noexcept {
    for (;;) __asm__ __volatile__("pause");
}
}
