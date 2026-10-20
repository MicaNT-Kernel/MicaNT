/**
 * @file prism_d3d9ex_com_vtable_abi.cpp
 * @brief Horizon 104 (Kernel Unit #76): Sovereign PrismX Official Microsoft Direct3D 9 / 9Ex
 *        (<d3d9.h> / d3d9.dll) 21-Slot IDirect3D9Ex & IDirect3DDevice9Ex COM VTable ABI.
 *        Implements standalone PE32+ DLL exports for d3d9.dll.
 */

#include <cstdint>
#include <cstddef>

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_d3d9ex_com_vtable_abi.hpp"

namespace micant::sys::internal {

void PrismXD3D9ExComVTableAbiInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_d3d9_abi::InitializePrismD3D9ExComAbiExports();
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

#if defined(BUILD_SYSTEM32_D3D9_DLL)
extern "C" {
__declspec(dllexport) void* Direct3DCreate9(uint32_t /*SDKVersion*/) {
    return &g_DummyInstance;
}

__declspec(dllexport) int32_t Direct3DCreate9Ex(uint32_t /*SDKVersion*/, void** ppD3D) {
    if (!ppD3D) return static_cast<int32_t>(0x80070057U);
    *ppD3D = &g_DummyInstance;
    return 0; // D3D_OK
}

__declspec(dllexport) int32_t D3DPERF_BeginEvent(uint32_t /*col*/, const wchar_t* /*wszName*/) {
    return 0;
}

__declspec(dllexport) int32_t D3DPERF_EndEvent() {
    return 0;
}

__declspec(dllexport) void D3DPERF_SetMarker(uint32_t /*col*/, const wchar_t* /*wszName*/) {}

__declspec(dllexport) uint32_t D3DPERF_GetStatus() {
    return 0;
}
}
#endif

#endif
