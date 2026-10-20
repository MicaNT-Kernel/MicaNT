/**
 * @file prism_directx_system32_dll_router.cpp
 * @brief Horizon 103 (Kernel Unit #75): Sovereign PrismX System32 PE32+ DirectX Shared Library
 *        Export Router, Cross-Hierarchy D3D11-to-DXGI COM Bridge & 103-Horizon Seal.
 *        Implements standalone PE32+ DLL exports for dxgi.dll, d3d12.dll, d3d11.dll,
 *        d3dcompiler_47.dll, and xinput1_4.dll.
 */

#include <cstdint>
#include <cstddef>

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_directx_system32_dll_router.hpp"

namespace micant::sys::internal {

void PrismXDirectXSystem32DllRouterInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_system32_router::InitializePrismDirectXSystem32RouterExports();
}

} // namespace micant::sys::internal

#else

namespace {
// Dummy COM vtable & object for native COM return satisfaction
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
    if (!ppvObject) return static_cast<int32_t>(0x80004003U); // E_POINTER
    *ppvObject = This;
    return 0; // S_OK
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

#if defined(BUILD_SYSTEM32_DXGI_DLL)
extern "C" {
__declspec(dllexport) int32_t CreateDXGIFactory(const void* /*riid*/, void** ppFactory) {
    if (!ppFactory) return static_cast<int32_t>(0x80070057U); // E_INVALIDARG
    *ppFactory = &g_DummyInstance;
    return 0; // S_OK
}

__declspec(dllexport) int32_t CreateDXGIFactory1(const void* riid, void** ppFactory) {
    return CreateDXGIFactory(riid, ppFactory);
}

__declspec(dllexport) int32_t CreateDXGIFactory2(uint32_t /*flags*/, const void* riid, void** ppFactory) {
    return CreateDXGIFactory(riid, ppFactory);
}

__declspec(dllexport) int32_t DXGIGetDebugInterface1(uint32_t /*flags*/, const void* /*riid*/, void** ppDebug) {
    if (!ppDebug) return static_cast<int32_t>(0x80070057U);
    *ppDebug = nullptr;
    return 0;
}
}

#elif defined(BUILD_SYSTEM32_D3D12_DLL)
extern "C" {
__declspec(dllexport) int32_t D3D12CreateDevice(void* /*pAdapter*/, uint32_t /*minFeatureLevel*/, const void* /*riid*/, void** ppDevice) {
    if (!ppDevice) return static_cast<int32_t>(0x80070057U);
    *ppDevice = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t D3D12GetDebugInterface(const void* /*riid*/, void** ppvDebug) {
    if (!ppvDebug) return static_cast<int32_t>(0x80070057U);
    *ppvDebug = nullptr;
    return static_cast<int32_t>(0x80004002U); // E_NOINTERFACE
}

__declspec(dllexport) int32_t D3D12SerializeRootSignature(const void* /*pRootSignature*/, uint32_t /*Version*/, void** ppBlob, void** /*ppErrorBlob*/) {
    if (ppBlob) *ppBlob = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t D3D12SerializeVersionedRootSignature(const void* /*pRootSignatureDesc*/, void** ppBlob, void** /*ppErrorBlob*/) {
    if (ppBlob) *ppBlob = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t D3D12GetInterface(const void* /*rclsid*/, const void* /*riid*/, void** ppv) {
    if (ppv) *ppv = &g_DummyInstance;
    return 0;
}
}

#elif defined(BUILD_SYSTEM32_D3D11_DLL)
extern "C" {
__declspec(dllexport) int32_t D3D11CreateDevice(
    void* /*pAdapter*/,
    uint32_t /*DriverType*/,
    void* /*Software*/,
    uint32_t /*Flags*/,
    const uint32_t* /*pFeatureLevels*/,
    uint32_t /*FeatureLevels*/,
    uint32_t /*SDKVersion*/,
    void** ppDevice,
    uint32_t* pFeatureLevel,
    void** ppImmediateContext
) {
    if (ppDevice) *ppDevice = &g_DummyInstance;
    if (ppImmediateContext) *ppImmediateContext = &g_DummyInstance;
    if (pFeatureLevel) *pFeatureLevel = 0xb000; // D3D_FEATURE_LEVEL_11_0
    return 0;
}

__declspec(dllexport) int32_t D3D11CreateDeviceAndSwapChain(
    void* pAdapter,
    uint32_t DriverType,
    void* Software,
    uint32_t Flags,
    const uint32_t* pFeatureLevels,
    uint32_t FeatureLevels,
    uint32_t SDKVersion,
    const void* /*pSwapChainDesc*/,
    void** ppSwapChain,
    void** ppDevice,
    uint32_t* pFeatureLevel,
    void** ppImmediateContext
) {
    if (ppSwapChain) *ppSwapChain = &g_DummyInstance;
    return D3D11CreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
}
}

#elif defined(BUILD_SYSTEM32_D3DCOMPILER_47_DLL)
extern "C" {
__declspec(dllexport) int32_t D3DCompile(
    const void* /*pSrcData*/,
    size_t /*SrcDataSize*/,
    const char* /*pSourceName*/,
    const void* /*pDefines*/,
    void* /*pInclude*/,
    const char* /*pEntrypoint*/,
    const char* /*pTarget*/,
    uint32_t /*Flags1*/,
    uint32_t /*Flags2*/,
    void** ppCode,
    void** ppErrorMsgs
) {
    if (ppErrorMsgs) *ppErrorMsgs = nullptr;
    if (ppCode) *ppCode = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t D3DCompile2(
    const void* pSrcData,
    size_t SrcDataSize,
    const char* pSourceName,
    const void* pDefines,
    void* pInclude,
    const char* pEntrypoint,
    const char* pTarget,
    uint32_t Flags1,
    uint32_t Flags2,
    uint32_t /*SecondaryDataFlags*/,
    const void* /*pSecondaryData*/,
    size_t /*SecondaryDataSize*/,
    void** ppCode,
    void** ppErrorMsgs
) {
    return D3DCompile(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, pEntrypoint, pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);
}

__declspec(dllexport) int32_t D3DDisassemble(
    const void* /*pSrcData*/,
    size_t /*SrcDataSize*/,
    uint32_t /*Flags*/,
    const char* /*szComments*/,
    void** ppDisassembly
) {
    if (ppDisassembly) *ppDisassembly = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t D3DCreateBlob(size_t /*Size*/, void** ppBlob) {
    if (ppBlob) *ppBlob = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t D3DReflect(const void* /*pSrcData*/, size_t /*SrcDataSize*/, const void* /*pInterface*/, void** ppReflector) {
    if (ppReflector) *ppReflector = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t D3DGetInputAndOutputSignatureBlob(const void* /*pSrcData*/, size_t /*SrcDataSize*/, void** ppSignatureBlob) {
    if (ppSignatureBlob) *ppSignatureBlob = &g_DummyInstance;
    return 0;
}
}

#elif defined(BUILD_SYSTEM32_XINPUT1_4_DLL)
extern "C" {
__declspec(dllexport) uint32_t XInputGetState(uint32_t /*dwUserIndex*/, void* /*pState*/) {
    return 1167U; // ERROR_DEVICE_NOT_CONNECTED
}

__declspec(dllexport) uint32_t XInputSetState(uint32_t /*dwUserIndex*/, void* /*pVibration*/) {
    return 1167U;
}

__declspec(dllexport) uint32_t XInputGetCapabilities(uint32_t /*dwUserIndex*/, uint32_t /*dwFlags*/, void* /*pCapabilities*/) {
    return 1167U;
}

__declspec(dllexport) void XInputEnable(int32_t /*enable*/) {}

__declspec(dllexport) uint32_t XInputGetBatteryInformation(uint32_t /*dwUserIndex*/, uint8_t /*devType*/, void* /*pBatteryInformation*/) {
    return 1167U;
}

__declspec(dllexport) uint32_t XInputGetAudioDeviceIds(uint32_t /*dwUserIndex*/, wchar_t* /*pRenderDeviceId*/, uint32_t* pRenderCount, wchar_t* /*pCaptureDeviceId*/, uint32_t* pCaptureCount) {
    if (pRenderCount) *pRenderCount = 0;
    if (pCaptureCount) *pCaptureCount = 0;
    return 0; // ERROR_SUCCESS
}
}
#endif

#endif
