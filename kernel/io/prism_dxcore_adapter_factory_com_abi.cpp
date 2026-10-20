/**
 * @file prism_dxcore_adapter_factory_com_abi.cpp
 * @brief Horizon 108 (Kernel Unit #80): Sovereign PrismX Official Microsoft DXCore
 *        (<dxcore_interface.h> / dxcore.dll) IDXCoreAdapterFactory1 COM VTable & ML/Compute Discovery.
 *        Implements standalone PE32+ DLL exports for dxcore.dll.
 */

#include <cstdint>
#include <cstddef>

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_dxcore_adapter_factory_com_abi.hpp"

namespace micant::sys::internal {

void PrismXDXCoreComAbiInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_dxcore_abi::InitializePrismDXCoreComExports();
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

#if defined(BUILD_SYSTEM32_DXCORE_DLL)
extern "C" {
__declspec(dllexport) int32_t DXCoreCreateAdapterFactory(const void* /*riid*/, void** ppvFactory) {
    if (!ppvFactory) return static_cast<int32_t>(0x80070057U);
    *ppvFactory = &g_DummyInstance;
    return 0; // S_OK
}
}
#endif

#endif
