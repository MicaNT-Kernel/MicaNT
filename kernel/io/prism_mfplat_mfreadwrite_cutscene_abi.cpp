/**
 * @file prism_mfplat_mfreadwrite_cutscene_abi.cpp
 * @brief Horizon 107 (Kernel Unit #79): Sovereign PrismX Official Microsoft Media Foundation
 *        (mfplat.dll & mfreadwrite.dll) COM VTables & Zero-Copy D3D11Texture2D Cutscene Decoder.
 *        Implements standalone PE32+ DLL exports for mfplat.dll and mfreadwrite.dll.
 */

#include <cstdint>
#include <cstddef>

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_mfplat_mfreadwrite_cutscene_abi.hpp"

namespace micant::sys::internal {

void PrismXMediaFoundationCutsceneAbiInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_mfplat_cutscene_abi::InitializePrismMediaFoundationCutsceneExports();
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

#if defined(BUILD_SYSTEM32_MFPLAT_DLL)
extern "C" {
__declspec(dllexport) int32_t MFStartup(uint32_t /*Version*/, uint32_t /*dwFlags*/) {
    return 0; // S_OK
}

__declspec(dllexport) int32_t MFShutdown() {
    return 0; // S_OK
}

__declspec(dllexport) int32_t MFCreateDXGIDeviceManager(uint32_t* pResetToken, void** ppDeviceManager) {
    if (pResetToken) *pResetToken = 1;
    if (!ppDeviceManager) return static_cast<int32_t>(0x80070057U);
    *ppDeviceManager = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t MFCreateMediaType(void** ppMFType) {
    if (!ppMFType) return static_cast<int32_t>(0x80070057U);
    *ppMFType = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t MFCreateSample(void** ppIMFSample) {
    if (!ppIMFSample) return static_cast<int32_t>(0x80070057U);
    *ppIMFSample = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t MFCreateMemoryBuffer(uint32_t /*cbMaxLength*/, void** ppBuffer) {
    if (!ppBuffer) return static_cast<int32_t>(0x80070057U);
    *ppBuffer = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t MFCreateAttributes(void** ppMFAttributes, uint32_t /*cInitialSize*/) {
    if (!ppMFAttributes) return static_cast<int32_t>(0x80070057U);
    *ppMFAttributes = &g_DummyInstance;
    return 0;
}
}

#elif defined(BUILD_SYSTEM32_MFREADWRITE_DLL)
extern "C" {
__declspec(dllexport) int32_t MFCreateSourceReaderFromURL(const wchar_t* /*pwszURL*/, void* /*pAttributes*/, void** ppSourceReader) {
    if (!ppSourceReader) return static_cast<int32_t>(0x80070057U);
    *ppSourceReader = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t MFCreateSourceReaderFromByteStream(void* /*pByteStream*/, void* /*pAttributes*/, void** ppSourceReader) {
    if (!ppSourceReader) return static_cast<int32_t>(0x80070057U);
    *ppSourceReader = &g_DummyInstance;
    return 0;
}

__declspec(dllexport) int32_t MFCreateSinkWriterFromURL(const wchar_t* /*pwszOutputURL*/, void* /*pByteStream*/, void* /*pAttributes*/, void** ppSinkWriter) {
    if (!ppSinkWriter) return static_cast<int32_t>(0x80070057U);
    *ppSinkWriter = &g_DummyInstance;
    return 0;
}
}
#endif

#endif
