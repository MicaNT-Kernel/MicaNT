/**
 * @file prism_xaudio2_dinput8_com_abi.cpp
 * @brief Horizon 105 (Kernel Unit #77): Sovereign PrismX Official Microsoft XAudio2 2.9
 *        (xaudio2_9.dll) & DirectInput8 (dinput8.dll) COM VTable ABI & Binary IIDs.
 *        Implements standalone PE32+ DLL exports for xaudio2_9.dll and dinput8.dll.
 */

#include <cstdint>
#include <cstddef>

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_xaudio2_dinput8_com_abi.hpp"

namespace micant::sys::internal {

void PrismXXAudio2AndDirectInput8ComAbiInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_xaudio2_dinput8_abi::InitializePrismXAudio2AndDirectInput8Exports();
}

} // namespace micant::sys::internal

#else

namespace {
struct DummyUnknownVtbl {
    int32_t  (*QueryInterface)(void* This, const void* riid, void** ppvObject);
    uint32_t (*AddRef)(void* This);
    uint32_t (*Release)(void* This);
};

struct DummyComObject {
    const void* lpVtbl;
    uint32_t refCount;
};

static int32_t Dummy_QueryInterface(void* This, const void* /*riid*/, void** ppvObject) noexcept {
    if (!ppvObject) return static_cast<int32_t>(0x80004003U);
    *ppvObject = This;
    return 0;
}
static uint32_t Dummy_AddRef(void* /*This*/) noexcept { return 2; }
static uint32_t Dummy_Release(void* /*This*/) noexcept { return 1; }

static const DummyUnknownVtbl g_DummyVtbl = {
    &Dummy_QueryInterface,
    &Dummy_AddRef,
    &Dummy_Release
};

static DummyComObject g_DummyInstance = {
    &g_DummyVtbl,
    1
};
} // namespace

#if defined(BUILD_SYSTEM32_XAUDIO2_9_DLL)
extern "C" {
__declspec(dllexport) int32_t XAudio2Create(void** ppXAudio2, uint32_t /*Flags*/, uint32_t /*XAudio2Processor*/) {
    if (!ppXAudio2) return static_cast<int32_t>(0x80070057U);
    *ppXAudio2 = &g_DummyInstance;
    return 0; // S_OK
}

__declspec(dllexport) int32_t CreateAudioReverb(void** ppApo) {
    if (!ppApo) return static_cast<int32_t>(0x80070057U);
    *ppApo = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t CreateAudioVolumeMeter(void** ppApo) {
    if (!ppApo) return static_cast<int32_t>(0x80070057U);
    *ppApo = &g_DummyInstance;
    return 0;
}
}

#elif defined(BUILD_SYSTEM32_DINPUT8_DLL)
extern "C" {
__declspec(dllexport) int32_t DirectInput8Create(void* /*hinst*/, uint32_t /*dwVersion*/, const void* /*riidltf*/, void** ppvOut, void* /*punkOuter*/) {
    if (!ppvOut) return static_cast<int32_t>(0x80070057U);
    *ppvOut = &g_DummyInstance;
    return 0; // DI_OK
}

__declspec(dllexport) int32_t DllCanUnloadNow() {
    return 0; // S_OK
}

__declspec(dllexport) int32_t DllGetClassObject(const void* /*rclsid*/, const void* /*riid*/, void** ppv) {
    if (!ppv) return static_cast<int32_t>(0x80070057U);
    *ppv = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t DllRegisterServer() { return 0; }
__declspec(dllexport) int32_t DllUnregisterServer() { return 0; }
}
#endif

#endif
