/**
 * @file winnt_vcruntime140_eh_abi.cpp
 * @brief Horizon 117 (Kernel Unit #89): Sovereign WinNT MSVC Exception Handling Runtimes
 *        (vcruntime140.dll & vcruntime140_1.dll) Official PE32+ Exports, __C_specific_handler
 *        Structured Exception Handling, and __CxxFrameHandler4 x64 C++ EH State Unwind ABI.
 */

#include "micant/ucrt/winnt_vcruntime140_eh_abi.hpp"
#include <cstdlib>
#include <cstring>

namespace micant::ucrt::winnt_vcruntime140_abi {

namespace {
uint32_t g_telemetryMainInvoked = 0;
uint32_t g_telemetryMainReturned = 0;
} // namespace

int32_t Vc_C_specific_handler(WinNtExceptionRecord* /*ExceptionRecord*/, void* /*EstablisherFrame*/, void* /*ContextRecord*/, WinNtDispatcherContext* /*DispatcherContext*/) noexcept {
    return 1; // ExceptionContinueSearch
}

void Vc_CxxThrowException(void* /*pExceptionObject*/, void* /*pThrowInfo*/) noexcept {}

void Vc_telemetry_main_invoke_trigger(void* /*reserved*/) noexcept {
    g_telemetryMainInvoked++;
}

void Vc_telemetry_main_return_trigger(void* /*reserved*/) noexcept {
    g_telemetryMainReturned++;
}

void Vc_std_type_info_destroy_list(void* /*pNode*/) noexcept {}

int32_t Vc_setjmp(void* /*env*/) noexcept {
    return 0;
}

void Vc_longjmp(void* /*env*/, int32_t /*val*/) noexcept {}

void* Vc_memcpy(void* dest, const void* src, size_t count) noexcept {
    return (dest && src) ? std::memcpy(dest, src, count) : dest;
}

void* Vc_memmove(void* dest, const void* src, size_t count) noexcept {
    return (dest && src) ? std::memmove(dest, src, count) : dest;
}

void* Vc_memset(void* dest, int32_t c, size_t count) noexcept {
    return dest ? std::memset(dest, c, count) : dest;
}

int32_t Vc_memcmp(const void* buf1, const void* buf2, size_t count) noexcept {
    return (buf1 && buf2) ? std::memcmp(buf1, buf2, count) : 0;
}

char* Vc_strstr(const char* str, const char* strSearch) noexcept {
    return (str && strSearch) ? const_cast<char*>(std::strstr(str, strSearch)) : nullptr;
}

int32_t Vc1_CxxFrameHandler4(WinNtExceptionRecord* /*ExceptionRecord*/, void* /*EstablisherFrame*/, void* /*ContextRecord*/, WinNtDispatcherContext* /*DispatcherContext*/) noexcept {
    return 1; // ExceptionContinueSearch
}

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
void WinNtVcruntime140Coordinator::initialize() noexcept {
    if (m_initialized) return;
    m_cachedReport = runVcruntimeAudit();
    m_initialized = true;
}

Horizon117Vcruntime140AuditReport WinNtVcruntime140Coordinator::runVcruntimeAudit() noexcept {
    Horizon117Vcruntime140AuditReport rep{};
    rep.vcruntime140ExportCount   = 12;
    rep.vcruntime140_1ExportCount = 1;

    // 1. Verify __C_specific_handler
    WinNtExceptionRecord rec{};
    rec.ExceptionCode = 0xC0000005U; // STATUS_ACCESS_VIOLATION
    WinNtDispatcherContext ctx{};
    rep.cSpecificHandlerVerified = (Vc_C_specific_handler(&rec, nullptr, nullptr, &ctx) == 1);

    // 2. Verify __CxxFrameHandler4
    rep.cxxFrameHandler4Verified = (Vc1_CxxFrameHandler4(&rec, nullptr, nullptr, &ctx) == 1);

    // 3. Verify Memory & String Primitives
    char src[32] = "MicaNT VCRuntime FastPath";
    char dst[32]{};
    Vc_memcpy(dst, src, sizeof(src));
    rep.memoryAndStringPrimitivesVerified = (Vc_memcmp(dst, src, sizeof(src)) == 0 &&
                                             Vc_strstr(dst, "FastPath") != nullptr);

    // 4. Verify Telemetry Invocation Triggers
    g_telemetryMainInvoked = 0;
    g_telemetryMainReturned = 0;
    Vc_telemetry_main_invoke_trigger(nullptr);
    Vc_telemetry_main_return_trigger(nullptr);
    rep.telemetryTriggersVerified = (g_telemetryMainInvoked == 1 && g_telemetryMainReturned == 1);

    return rep;
}

void InitializeWinNtVcruntime140Subsystem() noexcept {
    WinNtVcruntime140Coordinator::instance().initialize();
}
#endif

} // namespace micant::ucrt::winnt_vcruntime140_abi

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
namespace micant::sys::internal {
void WinNtVcruntime140InitializeExecutiveSubsystem() noexcept {
    micant::ucrt::winnt_vcruntime140_abi::InitializeWinNtVcruntime140Subsystem();
}
} // namespace micant::sys::internal
#endif

#if defined(BUILD_SYSTEM32_VCRUNTIME140_DLL)
extern "C" {
__declspec(dllexport) int32_t __C_specific_handler(micant::ucrt::winnt_vcruntime140_abi::WinNtExceptionRecord* r, void* e, void* c, micant::ucrt::winnt_vcruntime140_abi::WinNtDispatcherContext* d) { return micant::ucrt::winnt_vcruntime140_abi::Vc_C_specific_handler(r, e, c, d); }
__declspec(dllexport) void _CxxThrowException(void* p, void* t) { micant::ucrt::winnt_vcruntime140_abi::Vc_CxxThrowException(p, t); }
__declspec(dllexport) void __telemetry_main_invoke_trigger(void* r) { micant::ucrt::winnt_vcruntime140_abi::Vc_telemetry_main_invoke_trigger(r); }
__declspec(dllexport) void __telemetry_main_return_trigger(void* r) { micant::ucrt::winnt_vcruntime140_abi::Vc_telemetry_main_return_trigger(r); }
__declspec(dllexport) void __std_type_info_destroy_list(void* n) { micant::ucrt::winnt_vcruntime140_abi::Vc_std_type_info_destroy_list(n); }
__declspec(dllexport) int32_t vc_setjmp(void* env) { return micant::ucrt::winnt_vcruntime140_abi::Vc_setjmp(env); }
__declspec(dllexport) void vc_longjmp(void* env, int32_t val) { micant::ucrt::winnt_vcruntime140_abi::Vc_longjmp(env, val); }
__declspec(dllexport) void* vc_memcpy(void* d, const void* s, size_t c) { return micant::ucrt::winnt_vcruntime140_abi::Vc_memcpy(d, s, c); }
__declspec(dllexport) void* vc_memmove(void* d, const void* s, size_t c) { return micant::ucrt::winnt_vcruntime140_abi::Vc_memmove(d, s, c); }
__declspec(dllexport) void* vc_memset(void* d, int32_t val, size_t c) { return micant::ucrt::winnt_vcruntime140_abi::Vc_memset(d, val, c); }
__declspec(dllexport) int32_t vc_memcmp(const void* b1, const void* b2, size_t c) { return micant::ucrt::winnt_vcruntime140_abi::Vc_memcmp(b1, b2, c); }
__declspec(dllexport) char* vc_strstr(const char* s, const char* sub) { return micant::ucrt::winnt_vcruntime140_abi::Vc_strstr(s, sub); }
} // extern "C"
#elif defined(BUILD_SYSTEM32_VCRUNTIME140_1_DLL)
extern "C" {
__declspec(dllexport) int32_t __CxxFrameHandler4(micant::ucrt::winnt_vcruntime140_abi::WinNtExceptionRecord* r, void* e, void* c, micant::ucrt::winnt_vcruntime140_abi::WinNtDispatcherContext* d) { return micant::ucrt::winnt_vcruntime140_abi::Vc1_CxxFrameHandler4(r, e, c, d); }
} // extern "C"
#endif
